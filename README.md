## Simple STUN

### Simple high performance RFC 8489 compliant UDP IPv4 STUN server that just handles binding requests.

#### Install prerequisites (Ubuntu)

```
sudo apt install build-essential libboost-all-dev
```

#### Build

```
g++ -std=c++17 stunserver.cpp -o stunserver -lboost_system -lboost_thread -pthread
```

#### Run

For better performance avoid docker, load balancers and complex network setup.

```
while true; do
  ./stunserver
done
```
