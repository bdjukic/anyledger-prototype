//
// AnyLedger - Created by Bogdan Djukic on 01-04-2018.
//

#include "spark-dallas-temperature.h"
#include "OneWire.h"
#include "MQTT.h"

#include "Transaction.h"
#include "./helpers/CryptoHelper.h"

#include <vector>

static const uint8_t PRIVATE_KEY[] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                                      0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                                      0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                                      0x11, 0x11};

#define CONTRACT_ADDRESS "0x4f1c7fc11d8f35d1a385353d71af8e90013e38b2"
#define CHAIN_ID 3 // 0x1-mainnet, 0x3-Ropsten, 0x4-Rinkeby
#define GAS_PRICE 50000000000
#define GAS_LIMIT 100000
#define SMART_CONTRACT_METHOD "0x48f14470"
#define MQTT_BROKER_IP_ADDRESS "51.144.125.140"
#define MQTT_BROKER_PORT 1883

int totalTransactionCount = -1;

OneWire oneWire(D0);
DallasTemperature dallas(&oneWire);

void mqttBrokerCallback(char* topic, byte* payload, unsigned int length) {
  char payloadArray[length + 1];
  memcpy(payloadArray, payload, length);
  payloadArray[length] = NULL;

  String payloadRaw = String(payloadArray);

  // Parsing the response from INFURA
  totalTransactionCount = strtol(payloadRaw.substring(34, payloadRaw.length() - 2), NULL, 16);
  Serial.printlnf("Latest transaction count: %d", totalTransactionCount);
}

MQTT mqttClient(MQTT_BROKER_IP_ADDRESS, MQTT_BROKER_PORT, mqttBrokerCallback);

void loop() {
  if (mqttClient.isConnected())
    mqttClient.loop();

  dallas.requestTemperatures();
  int currentTemperature = (int)dallas.getTempCByIndex(0);

  vector<uint8_t> publicAddress = CryptoHelper::generateAddress(PRIVATE_KEY);
  vector<char> checksumAddress = CryptoHelper::generateChecksumAddress(publicAddress);

  string checksumAddressHex(checksumAddress.begin(), checksumAddress.end());

  Serial.printlnf("Public address: %s", checksumAddressHex.c_str());

  if (mqttClient.isConnected()) {
    mqttClient.publish("command/getTransactionCount", checksumAddressHex.c_str());

    Serial.printlnf("Sent request for latest transaction count.");
  }

  if (totalTransactionCount != -1) {
    Transaction transaction;
    transaction.setPrivateKey((uint8_t*)PRIVATE_KEY);

    uint32_t nonce = totalTransactionCount;
    uint32_t chainId = CHAIN_ID;
    uint32_t gasPrice = GAS_PRICE;
    uint32_t gasLimit = GAS_LIMIT;

    string to = CONTRACT_ADDRESS;
    string value = "";
    string method = "setTemperature(uint)";
    string data = "0xc604091c00000000000000000000000000000000000000000000000000000000000000" + string(String::format("%02x", currentTemperature));

    String rawTransaction = transaction.getRaw(nonce, gasPrice, gasLimit, &to, &value, &data, chainId);

    if (mqttClient.isConnected()) {
      mqttClient.publish("command/sendRawTransaction", rawTransaction);

      Serial.printlnf("Sent signed raw transaction.");
    }
  }

  delay(5000);
}

void setup() {
  Serial.begin(9600);

  Serial.printlnf("Connecting to MQTT broker...");
  mqttClient.connect("particle_" + String(Time.now()));

  if (mqttClient.isConnected()) {
    mqttClient.subscribe("command/getTransactionCountResult");
  }

  dallas.begin();
}
