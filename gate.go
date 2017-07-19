// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"sync"

	"github.com/ibelie/rpc"
	"github.com/ibelie/ruid"
)

const (
	GATE_NAME = "MICRO_GATE"
)

var (
	SYMBOL_GATE uint64
)

type GateImpl struct {
	mutex    sync.Mutex
	services map[ruid.RUID]string
}

var GateInst = GateImpl{services: make(map[ruid.RUID]string)}

func GateService(server rpc.IServer, symbols map[string]uint64) (uint64, rpc.Service) {
	return SYMBOL_GATE, &GateInst
}

func (s *GateImpl) Procedure(i ruid.RUID, method uint64, param []byte) (result []byte, err error) {
	return
}
