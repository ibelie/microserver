// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"log"
	"sync"

	"github.com/ibelie/rpc"
	"github.com/ibelie/ruid"
)

const (
	HUB_NAME = "MICRO_HUB"
)

var (
	SYMBOL_HUB uint64
)

type HubImpl struct {
	mutex    sync.Mutex
	services map[ruid.RUID]string
}

var HubInst = HubImpl{services: make(map[ruid.RUID]string)}

func HubService(server rpc.IServer, symbols map[string]uint64) (uint64, rpc.Service) {
	if _, exist := symbols[HUB_NAME]; exist {
		log.Fatalf("[Hub] Service '%s' already exists", HUB_NAME)
	}
	for _, s := range symbols {
		if SYMBOL_HUB <= s {
			SYMBOL_HUB = s + 1
		}
	}
	symbols[HUB_NAME] = SYMBOL_HUB
	ServerInst = server
	Symbols = symbols
	return SYMBOL_HUB, &HubInst
}

func (s *HubImpl) Procedure(i ruid.RUID, method uint64, param []byte) (result []byte, err error) {
	return
}
