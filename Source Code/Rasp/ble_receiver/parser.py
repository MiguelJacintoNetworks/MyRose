import logging
from . import (
    DFR0022_ID,
    SERVO_ID,
    PIR_ID,
    SPEAKER_ID,
    TEMP_ID,
    SOIL_ID,
    PUMP_ID,
)

def compute_checksum(packet: bytes) -> int:
    return sum(packet) & 0xFF

def parse_packet(packet: bytes):
    if len(packet) < 3:
        logging.error("[PARSER] PACKET TOO SHORT: %s", packet.hex().upper())
        return None

    sid = packet[0]

    if sid == DFR0022_ID:
        if len(packet) != 4:
            logging.error("[PARSER] INVALID DFR0022 PACKET LENGTH: %d (EXPECTED 4)", len(packet))
            return None
        chk = compute_checksum(packet[:3])
        if chk != packet[3]:
            logging.error("[PARSER] DFR0022 CHECKSUM ERROR: EXPECTED %02X, GOT %02X", chk, packet[3])
            return None
        raw = packet[1] | (packet[2] << 8)
        lux = (raw / 1023.0) * 6000.0
        lux_str = f"{lux:.1f}"
        return ("DFR0022", lux_str)

    elif sid == SERVO_ID:
        if len(packet) != 3:
            logging.error("[PARSER] INVALID SERVO PACKET LENGTH: %d (EXPECTED 3)", len(packet))
            return None
        chk = compute_checksum(packet[:2])
        if chk != packet[2]:
            logging.error("[PARSER] SERVO CHECKSUM ERROR: EXPECTED %02X, GOT %02X", chk, packet[2])
            return None
        return ("SERVO", "SUCCESS" if packet[1] == 1 else "FAIL")

    elif sid == PIR_ID:
        if len(packet) != 3:
            logging.error("[PARSER] INVALID PIR PACKET LENGTH: %d (EXPECTED 3)", len(packet))
            return None
        chk = compute_checksum(packet[:2])
        if chk != packet[2]:
            logging.error("[PARSER] PIR CHECKSUM ERROR: EXPECTED %02X, GOT %02X", chk, packet[2])
            return None
        return ("PIR", str(packet[1]))

    elif sid == SPEAKER_ID:
        if len(packet) != 3:
            logging.error("[PARSER] INVALID SPEAKER PACKET LENGTH: %d (EXPECTED 3)", len(packet))
            return None
        chk = compute_checksum(packet[:2])
        if chk != packet[2]:
            logging.error("[PARSER] SPEAKER CHECKSUM ERROR: EXPECTED %02X, GOT %02X", chk, packet[2])
            return None
        return ("SPEAKER", "SUCCESS" if packet[1] == 1 else "FAIL")

    elif sid == PUMP_ID:
        if len(packet) != 3:
            logging.error("[PARSER] INVALID PUMP PACKET LENGTH: %d (EXPECTED 3)", len(packet))
            return None
        chk = compute_checksum(packet[:2])
        if chk != packet[2]:
            logging.error("[PARSER] PUMP CHECKSUM ERROR: EXPECTED %02X, GOT %02X", chk, packet[2])
            return None
        return ("PUMP", "SUCCESS" if packet[1] == 1 else "FAIL")

    elif sid == TEMP_ID:
        if len(packet) != 4:
            logging.error("[PARSER] INVALID TEMP PACKET LENGTH: %d (EXPECTED 4)", len(packet))
            return None
        chk = compute_checksum(packet[:3])
        if chk != packet[3]:
            logging.error("[PARSER] TEMP CHECKSUM ERROR: EXPECTED %02X, GOT %02X", chk, packet[3])
            return None
        value = packet[1] | (packet[2] << 8)
        return ("TEMP", str(value))

    elif sid == SOIL_ID:
        if len(packet) != 4:
            logging.error("[PARSER] INVALID SOIL PACKET LENGTH: %d (EXPECTED 4)", len(packet))
            return None
        chk = compute_checksum(packet[:3])
        if chk != packet[3]:
            logging.error("[PARSER] SOIL CHECKSUM ERROR: EXPECTED %02X, GOT %02X", chk, packet[3])
            return None
        raw = packet[1] | (packet[2] << 8)
        percent = (raw / 1023.0) * 100.0
        percent_str = f"{percent:.1f}"
        return ("SOIL", percent_str)

    else:
        logging.error("[PARSER] UNKNOWN SENSOR ID: %d", sid)
        return None