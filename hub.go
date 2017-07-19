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
	HUB_NAME   = "MICRO_HUB"
	HUB_NOTIFY = "NOTIFY"
)

var (
	SYMBOL_HUB    uint64
	SYMBOL_NOTIFY uint64
)

type HubImpl struct {
	mutex    sync.Mutex
	services map[ruid.RUID]string
}

var HubInst = HubImpl{services: make(map[ruid.RUID]string)}

func HubService(server rpc.IServer, symbols map[string]uint64) (uint64, rpc.Service) {
	return SYMBOL_HUB, &HubInst
}

func (s *HubImpl) Procedure(i ruid.RUID, method uint64, param []byte) (result []byte, err error) {
	return
}
