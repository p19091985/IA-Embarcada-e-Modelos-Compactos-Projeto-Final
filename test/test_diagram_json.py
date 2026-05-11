import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def load_diagram():
    return json.loads((ROOT / "diagram.json").read_text(encoding="utf-8"))


def test_diagram_uses_expected_esp32s3_parts():
    diagram = load_diagram()
    parts = {part["id"]: part for part in diagram["parts"]}

    assert parts["esp"]["type"] == "board-esp32-s3-devkitc-1"
    assert parts["keypad1"]["type"] == "wokwi-membrane-keypad"
    assert parts["oled1"]["type"] == "board-ssd1306"
    assert parts["mpu1"]["type"] == "wokwi-mpu6050"
    assert parts["lcd1"]["type"] == "wokwi-lcd1602"

    for led_id in ["led2", "led3", "led4", "led5", "led6", "led7"]:
        assert parts[led_id]["type"] == "wokwi-led"

    for buzzer_id in ["bz1", "bz2", "bz3", "bz4"]:
        assert parts[buzzer_id]["type"] == "wokwi-buzzer"

    assert "dht1" not in parts


def test_gpio_mapping_matches_hardware_plan():
    diagram = load_diagram()
    connections = {(left, right) for left, right, *_ in diagram["connections"]}
    reverse = {(right, left) for left, right, *_ in diagram["connections"]}
    all_connections = connections | reverse

    expected = {
        ("keypad1:R1", "esp:2"),
        ("keypad1:R2", "esp:3"),
        ("keypad1:R3", "esp:4"),
        ("keypad1:R4", "esp:5"),
        ("keypad1:C1", "esp:6"),
        ("keypad1:C2", "esp:7"),
        ("keypad1:C3", "esp:8"),
        ("keypad1:C4", "esp:9"),
        ("led2:A", "esp:11"),
        ("led3:A", "esp:12"),
        ("led7:A", "esp:13"),
        ("oled1:SDA", "esp:14"),
        ("oled1:SCL", "esp:15"),
        ("mpu1:SDA", "esp:14"),
        ("mpu1:SCL", "esp:15"),
        ("lcd1:SDA", "esp:16"),
        ("lcd1:SCL", "esp:17"),
        ("bz4:2", "esp:18"),
    }

    assert expected <= all_connections


def test_mpu6050_is_powered_and_shares_oled_i2c_bus():
    diagram = load_diagram()
    connections = {(left, right) for left, right, *_ in diagram["connections"]}
    reverse = {(right, left) for left, right, *_ in diagram["connections"]}
    all_connections = connections | reverse

    expected = {
        ("mpu1:VCC", "esp:3V3"),
        ("mpu1:GND", "gnd2:GND"),
        ("mpu1:SDA", "esp:14"),
        ("mpu1:SCL", "esp:15"),
        ("oled1:SDA", "esp:14"),
        ("oled1:SCL", "esp:15"),
    }

    assert expected <= all_connections
