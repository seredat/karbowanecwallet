**1. Clone wallet sources**

```
git clone --recurse-submodule https://github.com/elyacoin/elyacoinwallet.git
```

**2. Update the sources

```
sh ./update-submodules.sh
```

**3. Build**

```
mkdir build && cd build && cmake .. && make
```
