// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"fmt"
	"log"
	"time"

	"encoding/base64"
	"github.com/ibelie/rpc"
	"golang.org/x/net/websocket"
	"net/http"
)

type WsConn struct {
	*websocket.Conn
}

func (c *WsConn) Address() string {
	return c.RemoteAddr().String()
}

func (c *WsConn) Send(data []byte) (err error) {
	message := base64.RawURLEncoding.EncodeToString(data)
	if err = c.Conn.SetWriteDeadline(time.Now().Add(time.Second * WRITE_DEADLINE)); err == nil {
		err = websocket.Message.Send(c.Conn, message)
	}
	return
}

func (c *WsConn) Receive() (data []byte, err error) {
	var message string
	if err = c.Conn.SetReadDeadline(time.Now().Add(time.Second * READ_DEADLINE)); err == nil {
		if err = websocket.Message.Receive(c.Conn, &message); err == nil {
			data, err = base64.RawURLEncoding.DecodeString(message)
		}
	}
	return
}

type WsNet int

var Websocket WsNet = 0

func (_ WsNet) Serve(address string, handler func(rpc.Connection)) {
	http.Handle("/", http.FileServer(http.Dir(".")))
	http.Handle("/mws", websocket.Handler(func(ws *websocket.Conn) {
		handler(&WsConn{Conn: ws})
	}))

	if err := http.ListenAndServe(address, nil); err != nil {
		log.Fatalf("[Websocket@%v] Cannot listen:\n>>>> %v", address, err)
	}
}

func (_ WsNet) Connect(address string) rpc.Connection {
	url := fmt.Sprintf("ws://%s/mws", address)
	origin := fmt.Sprintf("http://%s/", address)
	if conn, err := websocket.Dial(url, "", origin); err != nil {
		log.Printf("[Websocket] Connect %s failed:\n>>>> %v", address, err)
		return nil
	} else {
		return &TcpConn{Conn: conn}
	}
}
