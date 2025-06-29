import os
import time
import logging
from . import FILE_MAP

BASE_DIR   = os.path.dirname(os.path.dirname(__file__))
OUTPUT_DIR = os.path.join(BASE_DIR, 'output')
os.makedirs(OUTPUT_DIR, exist_ok=True)

def write_to_csv(sensor: str, data_str: str):
    fname    = FILE_MAP.get(sensor, "UNKNOWN.CSV")
    filepath = os.path.join(OUTPUT_DIR, fname)
    try:
        with open(filepath, "a") as f:
            f.write(f"{time.time()},{data_str}\n")
        logging.info("[WRITER] DATA WRITTEN TO %s", filepath)
    except Exception as e:
        logging.error("[WRITER] FILE WRITE ERROR: %s", str(e).upper())