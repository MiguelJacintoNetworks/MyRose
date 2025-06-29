import asyncio
import json
import logging
import time
from pathlib import Path

import paho.mqtt.client as mqtt
from paho.mqtt.client import CallbackAPIVersion

from .config import TB_HOST, MQTT_PORT, ACCESS_TOKEN, CA_CERT_PATH, CSV_DIR, INTERVAL_SEC

STATE_FILE = CSV_DIR / ".publisher_state.json"
CONFIRM_CSV = CSV_DIR / "telemetry_confirmations.csv"

TELEMETRY_TOPIC = "v1/devices/me/telemetry"
RPC_REQUEST_PREFIX = "v1/devices/me/rpc/request/"
RPC_RESPONSE_TOPIC = "v1/devices/me/rpc/response/+"
QOS = 1

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)

class DataPublisher:
    def __init__(self):
        self.state = self._load_state()
        self.rpc_request_id = 1

        self.client = mqtt.Client(
            client_id=ACCESS_TOKEN,
            protocol=mqtt.MQTTv5,
            callback_api_version=CallbackAPIVersion.VERSION2,
        )
        self.client.username_pw_set(ACCESS_TOKEN)

        if CA_CERT_PATH and Path(CA_CERT_PATH).is_file():
            self.client.tls_set(ca_certs=CA_CERT_PATH)
        else:
            self.client.tls_set()

        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

        self.client.connect(TB_HOST, MQTT_PORT, keepalive=60)
        self.client.loop_start()

        if not CONFIRM_CSV.exists():
            CONFIRM_CSV.write_text("TIMESTAMP,REQUEST_ID,RESPONSE\n")

        logging.info("[PUBLISHER] MQTT CLIENT INITIALIZED AND CONNECTING")

    def _on_connect(self, client, userdata, flags, rc, properties=None):
        client.subscribe(RPC_RESPONSE_TOPIC, qos=QOS)
        logging.info(f"[PUBLISHER] SUBSCRIBED TO RPC RESPONSES")

    def _on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode()
        if topic.startswith("v1/devices/me/rpc/response/"):
            request_id = topic.rsplit("/", 1)[-1]
            timestamp = int(time.time())
            CONFIRM_CSV.parent.mkdir(parents=True, exist_ok=True)
            CONFIRM_CSV.touch(exist_ok=True)
            with open(CONFIRM_CSV, "a") as f:
                f.write(f"{timestamp},{request_id},{payload}\n")
            logging.info(f"[PUBLISHER] RPC RESPONSE LOGGED: {request_id} → {payload}")

    def _load_state(self) -> dict:
        if STATE_FILE.exists():
            try:
                return json.loads(STATE_FILE.read_text())
            except Exception:
                logging.warning("[PUBLISHER] FAILED TO LOAD STATE FILE; STARTING FRESH")
        return {}

    def _save_state(self):
        try:
            STATE_FILE.write_text(json.dumps(self.state))
        except Exception as e:
            logging.error(f"[PUBLISHER] FAILED TO WRITE STATE FILE: {e}")

    def clear_offsets(self):
        self.state.clear()
        self._save_state()
        logging.info("[PUBLISHER] OFFSETS RESET; FULL CSV WILL BE SENT NEXT CYCLE")

    def send_rpc_request(self, method: str):
        request_id = str(self.rpc_request_id)
        self.rpc_request_id += 1

        payload = json.dumps({"method": method, "params": {}})
        topic = f"{RPC_REQUEST_PREFIX}{request_id}"

        self.client.publish(topic, payload, qos=QOS).wait_for_publish()
        logging.info(f"[PUBLISHER] RPC REQUEST SENT: {topic} → {payload}")

    def publish_all(self):
        for csv_path in CSV_DIR.glob("*_DATA.CSV"):
            key = csv_path.stem
            offset = self.state.get(key, 0)

            try:
                with csv_path.open("r") as f:
                    f.seek(offset)
                    for line in f:
                        ts_str, data_str = line.rstrip("\n").split(",", 1)
                        try:
                            ts = int(float(ts_str) * 1000)
                        except ValueError:
                            logging.warning(f"[PUBLISHER] INVALID TIMESTAMP IN {key}: {ts_str}")
                            continue

                        val = float(data_str) if data_str.replace(".", "", 1).isdigit() else data_str
                        payload = {"ts": ts, "values": {key: val}}
                        self.client.publish(TELEMETRY_TOPIC, json.dumps(payload), qos=QOS).wait_for_publish()
                        logging.info(f"[PUBLISHER] PUBLISHED TELEMETRY: {payload}")

                    self.state[key] = f.tell()

            except Exception as e:
                logging.error(f"[PUBLISHER] ERROR READING {key}: {e}")

        self._save_state()

async def publisher_loop():
    publisher = DataPublisher()
    while True:
        publisher.publish_all()
        publisher.send_rpc_request("getTelemetryMetrics")
        await asyncio.sleep(INTERVAL_SEC)

if __name__ == "__main__":
    asyncio.run(publisher_loop())