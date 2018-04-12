package main

import (
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"os/signal"
	"strings"

	"github.com/yosssi/gmq/mqtt"
	"github.com/yosssi/gmq/mqtt/client"
)

func main() {
	fmt.Println("Starting MQTT Client Proxy...")

	sigc := make(chan os.Signal, 1)
	signal.Notify(sigc, os.Interrupt, os.Kill)

	mqttClient := client.New(&client.Options{
		ErrorHandler: func(err error) {
			fmt.Println(err)
		},
	})

	defer mqttClient.Terminate()

	err := mqttClient.Connect(&client.ConnectOptions{
		Network:  "tcp",
		Address:  "51.144.125.140:1883",
		ClientID: []byte("go-client"),
	})
	if err != nil {
		panic(err)
	}

	fmt.Println("MQTT Client Proxy started.")

	err = mqttClient.Subscribe(&client.SubscribeOptions{
		SubReqs: []*client.SubReq{
			&client.SubReq{
				TopicFilter: []byte("command/sendRawTransaction"),
				QoS:         mqtt.QoS0,
				Handler: func(topicName, message []byte) {
					fmt.Println("New raw transaction received. Passing it to INFURA.")

					params := string(message[:])

					jsonData := fmt.Sprintf(` {"jsonrpc":"2.0", "method":"eth_sendRawTransaction", "params": ["%s"], "id":3}`, params)
					response, err := http.Post("https://ropsten.infura.io/k1qUYiOPy281bLyAQmDA", "application/json", strings.NewReader(jsonData))
					
					if err != nil {
						fmt.Printf("Request to INFURA failed with an error: %s\n", err)
						fmt.Println()
					} else {
						data, _ := ioutil.ReadAll(response.Body)
						
						fmt.Println("INFURA response:")
						fmt.Println(string(data))
						fmt.Println()
					}
				},
			},
			&client.SubReq {
				TopicFilter: []byte("command/getTransactionCount"),
				QoS:         mqtt.QoS0,
				Handler: func(topicName, message []byte) {
					fmt.Println("Request for transaction count received. Passing it to INFURA.")

					params := string(message[:])

					jsonData := fmt.Sprintf(` {"jsonrpc":"2.0", "method":"eth_getTransactionCount", "params": ["%s", "latest"], "id":3}`, params)
					response, err := http.Post("https://ropsten.infura.io/k1qUYiOPy281bLyAQmDA", "application/json", strings.NewReader(jsonData))
					
					if err != nil {
						fmt.Printf("Request to INFURA failed with an error: %s\n", err)
						fmt.Println()
					} else {
						data, _ := ioutil.ReadAll(response.Body)
						
						fmt.Println("INFURA response:")
						fmt.Println(string(data))
						fmt.Println()

						mqttClient.Publish(&client.PublishOptions{
							QoS:       mqtt.QoS0,
							TopicName: []byte("command/getTransactionCountResult"),
							Message:   []byte(data),
						})
					}
				},
			},
		},
	})

	err = mqttClient.Unsubscribe(&client.UnsubscribeOptions{
		TopicFilters: [][]byte{
			[]byte("command/sendRawTransaction"),
		},
	})
	if err != nil {
		panic(err)
	}

	err = mqttClient.Unsubscribe(&client.UnsubscribeOptions{
		TopicFilters: [][]byte{
			[]byte("command/getTransactionCount"),
		},
	})
	if err != nil {
		panic(err)
	}

	<-sigc

	if err := mqttClient.Disconnect(); err != nil {
		panic(err)
	}
}
