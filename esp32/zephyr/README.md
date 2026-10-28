# Zephyr build

## Environment

- Use [Zephyr Nix](https://github.com/nix-community/zephyr-nix) shell:

```
nix develop
```

- Create Python Virtual environment with dependencies:

```
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

```

- Fetch Zephyr

```
west init -l
```

## Build

- Check VSCode tasks

