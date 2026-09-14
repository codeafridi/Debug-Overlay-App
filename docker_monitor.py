import json
import shutil
import subprocess


def docker_available():
    """Return True if the Docker CLI is installed and usable."""
    return shutil.which("docker") is not None


def get_containers():
    """
    Return information about all Docker containers.

    Each container contains:
        name
        status
        health
        restarts
    """

    if not docker_available():
        return []

    try:
        result = subprocess.run(
            [
                "docker",
                "ps",
                "-aq",
            ],
            capture_output=True,
            text=True,
            timeout=3,
            check=True,
        )

        container_ids = result.stdout.strip().splitlines()

    except (subprocess.SubprocessError, OSError):
        return []

    containers = []

    for container_id in container_ids:
        try:
            inspect = subprocess.run(
                [
                    "docker",
                    "inspect",
                    container_id,
                ],
                capture_output=True,
                text=True,
                timeout=3,
                check=True,
            )

            data = json.loads(inspect.stdout)[0]

            state = data.get("State", {})

            health_data = state.get("Health")

            health = (
                health_data.get("Status", "none")
                if health_data
                else "none"
            )

            containers.append(
                {
                    "id": container_id,
                    "name": data.get("Name", "").lstrip("/"),
                    "status": state.get("Status", "unknown"),
                    "health": health,
                    "restarts": data.get("RestartCount", 0),
                    "exit_code": state.get("ExitCode", 0),
                }
            )

        except (
            subprocess.SubprocessError,
            OSError,
            json.JSONDecodeError,
            IndexError,
            AttributeError,
        ):
            continue

    return containers


def get_docker_summary():
    """Return a compact summary suitable for the overlay."""

    containers = get_containers()

    running = [
        container
        for container in containers
        if container["status"] == "running"
    ]

    unhealthy = [
        container
        for container in running
        if container["health"] == "unhealthy"
    ]

    restarting = [
        container
        for container in containers
        if container["status"] == "restarting"
    ]

    return {
        "available": docker_available(),
        "running": len(running),
        "unhealthy": unhealthy,
        "restarting": restarting,
        "containers": containers,
    }


def get_docker_summary_text(summary):
    if not summary["available"]:
        return "docker: unavailable"

    running = summary["running"]
    unhealthy = len(summary["unhealthy"])

    if running == 0:
        return "docker: 0 running"

    if unhealthy:
        return f"docker: {running} running | {unhealthy} unhealthy"

    return f"docker: {running} running | healthy"


if __name__ == "__main__":
    summary = get_docker_summary()

    if not summary["available"]:
        print("Docker: unavailable")
    elif not summary["containers"]:
        print("Docker: no containers")
    else:
        print(f"Docker: {summary['running']} running")

        for container in summary["containers"]:
            print(
                f"- {container['name']} | "
                f"{container['status']} | "
                f"health={container['health']} | "
                f"restarts={container['restarts']} | "
                f"exit={container['exit_code']}"
            )