// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"log"
	"sync"

	"github.com/ibelie/ruid"
)

const (
	GATE_NAME = "MICRO_GATE"
)

var (
	SYMBOL_GATE uint64
)

type GateService struct {
	mutex    sync.Mutex
	services map[ruid.RUID]string
}

var _GateService = GateService{services: make(map[ruid.RUID]string)}

func GateRegister(server IServer, symbols map[string]uint64) (uint64, *GateService) {
	if _, exist := symbols[GATE_NAME]; exist {
		log.Fatalf("[Gate] Service '%s' already exists", GATE_NAME)
	}
	for _, s := range symbols {
		if SYMBOL_GATE <= s {
			SYMBOL_GATE = s + 1
		}
	}
	symbols[GATE_NAME] = SYMBOL_GATE
	return SYMBOL_GATE, &_GateService
}

func (s *GateService) Procedure(i ruid.RUID, method uint64, param []byte) (result []byte, err error) {
	return
}
