import os
import sys
import logging
from pathlib import Path
import yaml

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)

BASE_DIR = Path(__file__).resolve().parent.parent
CFG_PATH = BASE_DIR / 'config' / 'config.yaml'

try:
    with CFG_PATH.open('r') as f:
        _cfg = yaml.safe_load(f)
except FileNotFoundError:
    logging.error(f"[CONFIG] CONFIGURATION FILE NOT FOUND: {CFG_PATH}")
    sys.exit(1)
except yaml.YAMLError as e:
    logging.error(f"[CONFIG] ERROR PARSING CONFIGURATION FILE: {e}")
    sys.exit(1)

def _get(section, key, default=None, required=False):
    sec = _cfg.get(section, {})
    if key in sec:
        return sec[key]
    if default is not None:
        return default
    if required:
        logging.error(f"[CONFIG] MISSING REQUIRED CONFIG: [{section}][{key}]")
        sys.exit(1)
    return None

TB_HOST = os.getenv('TB_HOST', _get('thingsboard', 'host', required=True))
ACCESS_TOKEN = os.getenv('TB_TOKEN', _get('thingsboard', 'token', required=True))

try:
    MQTT_PORT = int(os.getenv('TB_PORT', _get('thingsboard', 'port', 8883)))
except ValueError:
    logging.error("[CONFIG] MQTT_PORT MUST BE AN INTEGER")
    sys.exit(1)

CA_CERT_PATH = os.getenv('CA_CERT_PATH', _get('thingsboard', 'ca_cert', None))

raw_csv_dir = os.getenv('CSV_DIR', _get('publisher', 'csv_directory', required=True))
CSV_DIR = Path(raw_csv_dir)
if not CSV_DIR.is_absolute():
    CSV_DIR = BASE_DIR / CSV_DIR
if not CSV_DIR.exists() or not CSV_DIR.is_dir():
    logging.error(f"[CONFIG] INVALID OR MISSING CSV DIRECTORY: {CSV_DIR}")
    sys.exit(1)

try:
    INTERVAL_SEC = int(os.getenv('INTERVAL_SEC', _get('publisher', 'interval_seconds', required=True)))
    if INTERVAL_SEC <= 0:
        raise ValueError
except ValueError:
    logging.error("[CONFIG] INTERVAL_SECONDS MUST BE A POSITIVE INTEGER")
    sys.exit(1)

logging.info(f"[CONFIG] CONFIGURATION LOADED: TB_HOST = {TB_HOST}, MQTT_PORT = {MQTT_PORT}, CSV_DIR = {CSV_DIR}, INTERVAL_SEC = {INTERVAL_SEC}")