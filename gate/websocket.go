// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package gate

import (
	"log"
	"net/http"

	"encoding/base64"
	"github.com/ibelie/microserver"
	"golang.org/x/net/websocket"
)

type WebsocketGate struct {
	*websocket.Conn
}

func (g *WebsocketGate) Address() string {
	return g.RemoteAddr().String()
}

func (g *WebsocketGate) Send(data []byte) (err error) {
	message := base64.RawURLEncoding.EncodeToString(data)
	return websocket.Message.Send(g.Conn, message)
}

func (g *WebsocketGate) Receive() (data []byte, err error) {
	var message string
	if err = websocket.Message.Receive(g.Conn, &message); err == nil {
		data, err = base64.RawURLEncoding.DecodeString(message)
	}
	return
}

func Websocket(address string, handler func(microserver.Gate)) {
	http.Handle("/", http.FileServer(http.Dir(".")))
	http.Handle("/ws", websocket.Handler(func(ws *websocket.Conn) {
		handler(&WebsocketGate{Conn: ws})
	}))

	if err := http.ListenAndServe(address, nil); err != nil {
		log.Fatalf("[Websocket@%v] Cannot listen:\n>>>>%v", address, err)
	}
}
