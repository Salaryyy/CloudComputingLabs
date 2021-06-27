package participant

import (
	"fmt"
	"kvstore2pcsystem/conf"
	"kvstore2pcsystem/debug"
	"kvstore2pcsystem/kvstore"
	"kvstore2pcsystem/parse"
	"log"
	"net"
	"sync"
	"time"
)

type participant struct {
	CoorAdd *net.TCPAddr
	PartAdd *net.TCPAddr
	Conn    *net.TCPConn
	Kv      *kvstore.KV
	Version int64
}

func NewParticipant(conf conf.Conf) (part *participant) {
	coorIp := net.ParseIP(conf.CoorIp)
	partIp := net.ParseIP(conf.Part[0].PartIp)

	if coorIp == nil || partIp == nil {
		return nil
	}
	part = &participant{
		CoorAdd: &net.TCPAddr{IP: coorIp, Port: conf.CoorPort},
		PartAdd: &net.TCPAddr{IP: partIp, Port: conf.Part[0].PartPort},
		Conn:    nil,
		Kv:      kvstore.NewKV(),
		Version: 0,
	}
	return
}

func (p *participant) Connect() {
	i := 0
	for {
		i++
		if debug.Debug {
			log.Println("尝试连接", p.CoorAdd.IP.String(), "第", i, "次")
		}
		conn, err := net.DialTCP("tcp4", p.PartAdd, p.CoorAdd)
		if err == nil {
			p.Conn = conn
			_ = p.Conn.SetKeepAlive(true)
			_ = p.Conn.SetKeepAlivePeriod(time.Second)
			log.Println("---连接成功---")
			break
		}
		if debug.Debug {
			fmt.Println(err)
		}
		time.Sleep(500 * time.Millisecond)
	}
}

func (p *participant) Read(orderStr chan string) {
	i := 0
	for {
		i++
		buf := make([]byte, 1024)
		n, err := p.Conn.Read(buf)
		if err != nil {
			log.Println("协调者断开:", err, i)
			_ = p.Conn.Close()
			p.Connect()
			continue
		}
		str := string(buf[:n])
		orderStr <- str

	}
}

func (p *participant) Execute(orderStr chan string) {
	for {
		orderstr := <-orderStr
		order := parse.GetOrder(orderstr)
		fmt.Println(order)
		if order.Op == parse.GET {
			p.Write(p.Kv.Run(order))
		} else {
			p.Write("Done\r\n")
			orderstr = <-orderStr
			fmt.Println(orderstr)
			var ans string
			if orderstr == "Done\r\n" {
				ans = p.Kv.Run(order)
			} else {
				ans = "-ERROR\r\n"
			}
			p.Write(ans)
		}
	}
}

func (p *participant) Write(AnsStr string) {
	n, err := p.Conn.Write([]byte(AnsStr))
	if err != nil {
		log.Println("写入错误")
	} else {
		log.Println("写入了", n, "字节数据")
	}
}

func Run(conf conf.Conf) {
	part := NewParticipant(conf)
	if part == nil {
		panic("配置文件错误")
	}
	part.Connect()
	var wg sync.WaitGroup
	wg.Add(1)
	var orderStr = make(chan string)
	go part.Read(orderStr)
	go part.Execute(orderStr)
	//part.Write("你好")
	wg.Wait()
}
