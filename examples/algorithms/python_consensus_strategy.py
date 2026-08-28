"""NDJSON multi-robot strategy process example.

This is a protocol example for a future ExternalStrategyAdapter. It reads one
world-state object per line and writes one wheel-command object per line.
"""

import json
import math
import sys


def clamp(value, minimum=-1.0, maximum=1.0):
    return max(minimum, min(maximum, value))


def normalize_angle(angle):
    while angle > math.pi:
        angle -= 2.0 * math.pi
    while angle < -math.pi:
        angle += 2.0 * math.pi
    return angle


def step(message):
    goal = message.get("parameters", {}).get("goal", [0.0, 0.0])
    commands = []
    for robot in message.get("robots", []):
        x, y, heading = robot["pose"]
        desired_heading = math.atan2(goal[1] - y, goal[0] - x)
        heading_error = normalize_angle(desired_heading - heading)
        distance = math.hypot(goal[0] - x, goal[1] - y)
        forward = clamp(distance * 0.01, 0.0, 0.45)
        turn = clamp(heading_error * 0.35, -0.35, 0.35)
        commands.append({
            "id": robot["id"],
            "left": clamp(forward - turn),
            "right": clamp(forward + turn),
        })
    return {
        "type": "command",
        "sequence": message.get("sequence"),
        "commands": commands,
        "diagnostics": {"agent_count": len(commands)},
    }


for line in sys.stdin:
    try:
        request = json.loads(line)
        if request.get("type") == "step":
            print(json.dumps(step(request), separators=(",", ":")), flush=True)
    except Exception as error:
        print(json.dumps({"type": "error", "message": str(error)}), flush=True)
