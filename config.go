// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"fmt"
	"log"

	"encoding/json"
	"io/ioutil"
)

type Config struct {
	Project string
	IP      string
	Gate    int
	Port    int
	Etcd    int
}

type Configs struct {
	Configs map[string]*Config
}

func ReadConfig(name string, filename string) (config *Config) {
	configs := new(Configs)
	if bytes, err := ioutil.ReadFile(filename); err != nil {
		log.Fatalf("[Config] Read Config:\n>>>> %v", err)
	} else if err := json.Unmarshal(bytes, &configs); err != nil {
		log.Fatalf("[Config] JSON Unmarshal:\n>>>> %v", err)
	} else if conf, ok := configs.Configs[name]; !ok {
		log.Fatalf("[Config] Cannot find config %q in %q\n>>>> %v", name, filename, configs)
	} else {
		config = conf
	}
	if common, ok := configs.Configs["common"]; ok {
		if config.Project == "" {
			config.Project = common.Project
		}
		if config.IP == "" {
			config.IP = common.IP
		}
		if config.Gate == 0 {
			config.Gate = common.Gate
		}
		if config.Port == 0 {
			config.Port = common.Port
		}
		if config.Etcd == 0 {
			config.Etcd = common.Etcd
		}
	}
	return
}

func (c *Config) GateAddress() string {
	return fmt.Sprintf(":%d", c.Gate)
}

func (c *Config) Address() string {
	return fmt.Sprintf("%s:%d", c.IP, c.Port)
}
