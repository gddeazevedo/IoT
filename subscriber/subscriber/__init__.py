import paho.mqtt.client as mqtt
import json
from .repositories import dispositivos_repository, medidas_repository


IP    = "localhost"
PORT  = 1883
TOPIC = "cefet/iot"

def on_connect(client, userdata, flags, rc):
	print(f"Connected with result code {rc}")
	client.subscribe(TOPIC)


def on_message(client, userdata, msg):
	payload_raw = msg.payload.decode()
	print(f"Topic {msg.topic} - Payload: {payload_raw}")
	payload = json.loads(payload_raw)

	dispositivo = dispositivos_repository.get_dispositivo_by_id(payload["id"])

	if dispositivo is None:
		dispositivos_repository.create_dispositivo(payload["id"], payload["lat"], payload["long"])

	medidas_repository.create_medida(payload["temperatura"], payload["umidade"], payload["timestamp"], payload["id"])


def run():
	client = mqtt.Client()
	client.on_connect = on_connect
	client.on_message = on_message
	client.connect(IP, PORT, 60)	
	client.loop_forever()
