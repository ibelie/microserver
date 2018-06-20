// Copyright 2017-2018 ibelie, Chen Jie, Joungtao. All rights reserved.
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
	Entry   string
	IP      string
	Gate    int
	Port    int
	Etcd    int
}

func ReadConfigs(filename string) (configs map[string]*Config) {
	configs = make(map[string]*Config)
	if bytes, err := ioutil.ReadFile(filename); err != nil {
		log.Fatalf("[Config] Read Config:\n>>>> %v", err)
	} else if err := json.Unmarshal(bytes, &configs); err != nil {
		log.Fatalf("[Config] JSON Unmarshal:\n>>>> %v", err)
	} else if common, ok := configs["common"]; ok {
		delete(configs, "common")
		for _, config := range configs {
			config.Update(common)
		}
	}
	return
}

func ReadConfig(name string, filename string) (config *Config) {
	configs := make(map[string]*Config)
	if bytes, err := ioutil.ReadFile(filename); err != nil {
		log.Fatalf("[Config] Read Config:\n>>>> %v", err)
	} else if err := json.Unmarshal(bytes, &configs); err != nil {
		log.Fatalf("[Config] JSON Unmarshal:\n>>>> %v", err)
	} else if conf, ok := configs[name]; !ok {
		log.Fatalf("[Config] Cannot find config %q in %q\n>>>> %v", name, filename, configs)
	} else {
		config = conf
	}
	if common, ok := configs["common"]; ok {
		config.Update(common)
	}
	return
}

func (c *Config) Update(o *Config) {
	if c.Project == "" {
		c.Project = o.Project
	}
	if c.Entry == "" {
		c.Entry = o.Entry
	}
	if c.IP == "" {
		c.IP = o.IP
	}
	if c.Gate == 0 {
		c.Gate = o.Gate
	}
	if c.Port == 0 {
		c.Port = o.Port
	}
	if c.Etcd == 0 {
		c.Etcd = o.Etcd
	}
}

func (c *Config) GateAddress() string {
	return fmt.Sprintf(":%d", c.Gate)
}

func (c *Config) Address() string {
	return fmt.Sprintf("%s:%d", c.IP, c.Port)
}
