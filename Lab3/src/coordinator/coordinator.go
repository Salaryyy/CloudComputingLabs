package coordinator

import (
	"container/list"
	"fmt"
	"kvstore2pcsystem/conf"
	"kvstore2pcsystem/debug"
	"log"
	"net"
	"strconv"
	"sync"
	"time"
)

type partInfo struct {
	PartAddress string
	Conn        *net.TCPConn
	version     int64
	response    chan string
	shutdown    bool
}

type coordinator struct {
	CoorAdd  *net.TCPAddr
	PartList []partInfo
	TmpConn  *list.List
	orderStr chan string
	PartNum  int
	NowNum   int
}

var mutex sync.Mutex

const ERRORStr = "-ERROR\r\n"

func NewCoordinator(conf conf.Conf) (coor *coordinator) {
	str := ":" + strconv.Itoa(conf.CoorPort)
	tmpadd, err := net.ResolveTCPAddr("tcp4", str)
	if err != nil {
		panic("COOR配置文件错误")
	}
	var tmppartlist []partInfo
	for _, i := range conf.Part {
		str = i.PartIp + ":" + strconv.Itoa(i.PartPort)
		tmppartlist = append(tmppartlist, partInfo{
			PartAddress: str,
			Conn:        nil,
			version:     0,
			response:    nil,
			shutdown:    true,
		})
	}
	coor = &coordinator{
		CoorAdd:  tmpadd,
		PartList: tmppartlist,
		TmpConn:  list.New(),
		orderStr: make(chan string, 10),
		PartNum:  conf.PartNum,
	}
	return
}

func (c *coordinator) Distribute() {
	log.Println("分配函数启动")
	for {
		log.Println("等待任务")
		str := <-c.orderStr
		log.Println("开始分配任务")
		flag := false
		isGet := true
		for i := 0; i < 5; i++ {
			mutex.Lock()
			num := c.NowNum
			mutex.Unlock()
			if num != 0 {
				break
			}
			time.Sleep(time.Millisecond * 100)
		}

		for index, tmp := range c.PartList {
			if debug.Debug {
				log.Println("检查", tmp.PartAddress, tmp.shutdown)
			}
			if !tmp.shutdown {
				flag = true
				log.Println("分配任务到", tmp.PartAddress)
				_, err := c.PartList[index].Conn.Write([]byte(str))
				if err != nil {
					log.Println(tmp.PartAddress, "失去连接:", err)
				}
			}
		}
		var s string
		var count []int
		for index, tmp := range c.PartList {
			if !tmp.shutdown {
				s = <-c.PartList[index].response
				count = append(count, index)
				if debug.Debug {
					fmt.Println("来自", tmp.PartAddress, "的响应信息:")
					fmt.Println(s)
				}
			}
		}
		if s == "Done\r\n" {
			isGet = false
		}

		if !isGet {
			if debug.Debug {
				log.Println("进入2PC")
			}
			mutex.Lock()
			num := c.NowNum
			mutex.Unlock()
			if len(count) == num {
				str = "Done\r\n"
			} else {
				str = "UNDO\r\n"
			}
			for _, tmp := range count {
				if debug.Debug {
					log.Println("检查", c.PartList[tmp].PartAddress, c.PartList[tmp].shutdown)
				}
				if !c.PartList[tmp].shutdown {
					log.Println("回复响应到", c.PartList[tmp].PartAddress)
					_, err := c.PartList[tmp].Conn.Write([]byte(str))
					if err != nil {
						log.Println(c.PartList[tmp].PartAddress, "失去连接:", err)
					}
				}
			}
			for _, tmp := range count {
				if !c.PartList[tmp].shutdown {
					ss := <-c.PartList[tmp].response
					if debug.Debug {
						fmt.Println("来自", c.PartList[tmp].PartAddress, "的响应信息:")
						fmt.Println(ss)
					}
					s = ss
				}
			}
		}

		conn, ok := (c.TmpConn.Front().Value).(*net.TCPConn)
		if !ok {
			log.Println("返回客户端数据丢失")
		} else {
			var err error
			if flag {
				_, err = conn.Write([]byte(s))
			} else {
				_, err = conn.Write([]byte(ERRORStr))
			}
			if err != nil {
				log.Println("返回客户端失败:", err)
			}
			err = conn.Close()
			if err != nil {
				log.Println("关闭客户端连接失败:", err)
			}
			c.TmpConn.Remove(c.TmpConn.Front())
			log.Println("关闭客户端连接")
		}
	}
}

func (c *coordinator) RecvPart(conn *net.TCPConn, index int) {
	for {
		buf := make([]byte, 1024)
		n, err := conn.Read(buf)
		if err != nil {
			log.Println(c.PartList[index].PartAddress, "参与者断开")
			c.PartList[index].shutdown = true
			close(c.PartList[index].response)
			mutex.Lock()
			c.NowNum--
			mutex.Unlock()
			return
		}
		str := string(buf[:n])
		//if debug.Debug {
		//	fmt.Println(str)
		//}
		c.PartList[index].response <- str
	}
}

func (c *coordinator) RecvCli(conn *net.TCPConn) {
	var str string
	for {
		buf := make([]byte, 1024)
		_ = conn.SetReadDeadline(time.Now().Add(time.Second))
		n, err := conn.Read(buf)
		if err != nil {
			log.Println("读取结束", err)
			break
		}
		str += string(buf[:n])
		if debug.Debug {
			fmt.Println(str)
		}
	}
	if debug.Debug {
		log.Println("写入队列")
	}
	c.orderStr <- str
}

func (c *coordinator) Handler(conn *net.TCPConn) {
	if debug.Debug {
		log.Println("进入处理函数")
	}
	addr := conn.RemoteAddr().String()
	isPart := false
	for index, tmp := range c.PartList {
		if tmp.PartAddress == addr {
			c.PartList[index].Conn = conn
			if debug.Debug {
				log.Println(index, addr, "参与者连接")
			}
			c.PartList[index].response = make(chan string, 10)
			isPart = true
			c.PartList[index].shutdown = false
			mutex.Lock()
			c.NowNum++
			mutex.Unlock()
			go c.RecvPart(conn, index)
		}
	}
	if !isPart {
		if debug.Debug {
			log.Println(addr, "客户端连接")
		}
		c.TmpConn.PushBack(conn)
		go c.RecvCli(conn)
	}
}

func Run(conf conf.Conf) {
	coor := NewCoordinator(conf)
	listener, err := net.ListenTCP("tcp", coor.CoorAdd)
	if err != nil {
		panic("建立监听错误")
	}
	go coor.Distribute()
	for {
		myconn, err := listener.AcceptTCP()
		if err != nil {
			continue
		}
		if debug.Debug {
			log.Println("收到连接")
		}
		go coor.Handler(myconn)
	}
}
