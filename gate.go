// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"github.com/ibelie/ruid"
)

type GateService struct {
	mutex    sync.Mutex
	services map[ruid.RUID]string
}

var _GateService = GateService{services: make(map[ruid.RUID]string)}

func GateRegister(server IServer, symbols map[string]uint64) (uint64, *GateService) {
	return symbols[""], _GateService
}

func (s *GateService) Procedure(i ruid.RUID, method uint64, param []byte) (result []byte, err error) {
}
