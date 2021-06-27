package kvstore

import (
	"encoding/json"
	"kvstore2pcsystem/parse"
	"strconv"
)

const (
	OKStr  = "+OK\r\n"
	NILStr = "*1\r\n$3\r\nnil\r\n"
	RNStr  = "\r\n"
)

type KV struct {
	kv map[string][]string
}

func NewKV() *KV {
	s := &KV{kv: make(map[string][]string)}
	return s
}

func (k KV) BackUp() (backupstr string, ok bool) {
	result, err := json.Marshal(k.kv)
	if err != nil {
		ok = false
	}
	ok = true
	backupstr = string(result)
	return

}

func (k *KV) ReBackUp(data []byte) {
	err := json.Unmarshal(data, &k.kv)
	if err != nil {
		panic("备份错误")
	}
}

func (k *KV) Run(order parse.Order) (ansStr string) {
	switch order.Op {
	case parse.SET:
		k.kv[order.Key] = order.Value
		ansStr = OKStr
	case parse.GET:
		if key, ok := k.kv[order.Key]; !ok {
			ansStr = NILStr
		} else {
			num := len(key)
			ansStr = "*" + strconv.Itoa(num) + RNStr
			for _, tmp := range key {
				num = len(tmp)
				ansStr += "$" + strconv.Itoa(num) + RNStr + tmp + RNStr
			}
		}
	case parse.DEL:
		num := 0
		for _, tmp := range order.Value {
			if _, ok := k.kv[tmp]; !ok {
				continue
			} else {
				num++
				delete(k.kv, tmp)
			}
		}
		delete(k.kv, "12")
		ansStr = ":" + strconv.Itoa(num) + RNStr
	}
	return
}
