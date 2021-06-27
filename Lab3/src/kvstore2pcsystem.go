package main

import (
	"errors"
	"kvstore2pcsystem/conf"
	"kvstore2pcsystem/coordinator"
	"kvstore2pcsystem/participant"
	"os"
)

func main() {
	if len(os.Args) != 3 {
		panic(errors.New("参数错误"))
	}
	//for i, v := range os.Args {
	//	fmt.Printf("args[%v]=%v\n", i, v)
	//}
	conf := conf.GetConf(os.Args[2])
	if conf.IsCoor {
		coordinator.Run(conf)
	} else {
		participant.Run(conf)
	}
	//orderSET:=parse.GetOrder("*4\r\n$3\r\nSET\r\n$7\r\nCS06142\r\n$5\r\nCloud\r\n$9\r\nComputing\r\n")
	//orderGET1:=parse.GetOrder("*2\r\n$3\r\nGET\r\n$7\r\nCS06142\r\n")
	//orderGET2:=parse.GetOrder("*2\r\n$3\r\nGET\r\n$7\r\nCS0142\r\n")
	//orderDEL:=parse.GetOrder("*3\r\n$3\r\nDEL\r\n$7\r\nCS06142\r\n$5\r\nCS162\r\n")
	//fmt.Println(str)
	//k:=kvstore.NewKV()
	//fmt.Println(orderSET)
	//fmt.Println(k.Run(orderSET))
	//fmt.Println(k)
	//fmt.Println(k.Run(orderGET1))
	//fmt.Println(k)
	//fmt.Println(k.Run(orderGET2))
	//fmt.Println(k)
	//fmt.Println(k.Run(orderDEL))
	//fmt.Println(k)
	//cmd:=exec.Command("./participant.exe")
	//if err:=cmd.Start();err!=nil{
	//	panic("执行错误")
	//}
	//cmd.StdinPipe()
}
