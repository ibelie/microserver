// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"log"
	"time"

	"encoding/json"
	"github.com/coreos/etcd/client"
	"github.com/ibelie/rpc"
	"golang.org/x/net/context"
)

func Discover(s rpc.Server, etcd string, namespace string) {
	cfg := client.Config{
		Endpoints:               []string{etcd},
		Transport:               client.DefaultTransport,
		HeaderTimeoutPerRequest: time.Second,
	}

	if c, err := client.New(cfg); err != nil {
		log.Fatalf("[MicroServer@%v] Cannot connect to etcd:\n>>>> %v", s.Address(), err)
	} else {
		api := client.NewKeysAPI(c)
		go keep(s, namespace, api)
		go watch(s, namespace, api)
	}
}

func keep(s rpc.Server, namespace string, api client.KeysAPI) {
	key := namespace + s.Address()
	value, _ := json.Marshal(s.GetNode())
	for {
		if _, err := api.Set(context.Background(), key, string(value), &client.SetOptions{TTL: time.Second * 10}); err != nil {
			log.Printf("[MicroServer@%v] Update server info:\n>>>> %v", s.Address(), err)
		}
		time.Sleep(time.Second * 3)
	}
}

func watch(s rpc.Server, namespace string, api client.KeysAPI) {
	watcher := api.Watcher(namespace, &client.WatcherOptions{Recursive: true})
	for {
		if res, err := watcher.Next(context.Background()); err == nil {
			if res.Action == "expire" || res.Action == "delete" {
				s.Remove(res.Node.Key)
			} else if res.Action == "set" || res.Action == "update" {
				node := new(rpc.Node)
				if err := json.Unmarshal([]byte(res.Node.Value), node); err != nil {
					log.Printf("[MicroServer@%v] Parse node value:\n>>>> %v", s.Address(), err)
					break
				}
				s.Add(res.Node.Key, node)
			}
		} else {
			log.Printf("[MicroServer@%v] Watch servers:\n>>>> %v", s.Address(), err)
		}
	}
}
