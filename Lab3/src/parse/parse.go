package parse

import (
	"strings"
)

type opType int32

const (
	SET opType = 0
	GET opType = 1
	DEL opType = 2
)

type Order struct {
	Op    opType
	Key   string
	Value []string
}

func GetOrder(orderStr string) (order Order) {
	data := strings.Split(orderStr, "\r\n")
	//opNum,_:=strconv.Atoi(data[0][1:])
	i := 6
	//fmt.Println(data)
	//fmt.Println("data是",data,len(data))
	switch data[2] {
	case "SET":
		order.Op = SET
	case "GET":
		order.Op = GET
	case "DEL":
		order.Op = DEL
		i = 4
	}
	order.Key = data[4]
	for ; i < len(data); i += 2 {
		order.Value = append(order.Value, data[i])
	}
	return
}
