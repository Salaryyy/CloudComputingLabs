package conf

import (
	"io/ioutil"
	"strconv"
	"strings"
)

type Part struct {
	PartIp   string
	PartPort int
}

type Conf struct {
	IsCoor   bool
	CoorIp   string
	CoorPort int
	PartNum  int
	Part     []Part
}

func GetConf(filename string) (conf Conf) {
	data, err := ioutil.ReadFile(filename)
	if err != nil {
		panic("文件错误")
	}
	//fmt.Println(string(data))
	file := strings.Split(string(data), "\n")
	for _, str := range file {
		//fmt.Printf("%s",str)
		if len(str) > 0 && str[0] != '!' {
			//fmt.Println(str)
			if str[:4] == "mode" {
				conf.IsCoor = str[5:16] == "coordinator"
			} else if str[:11] == "coordinator" {
				tmp := strings.Split(str[17:], ":")
				tmp[1] = strings.TrimSpace(tmp[1])
				conf.CoorIp = tmp[0]
				conf.CoorPort, _ = strconv.Atoi(tmp[1])
			} else {
				conf.PartNum++
				tmp := strings.Split(str[17:], ":")
				tmp[1] = strings.TrimSpace(tmp[1])
				port, _ := strconv.Atoi(tmp[1])
				conf.Part = append(conf.Part, Part{tmp[0], port})
			}
		}
	}
	return
}
