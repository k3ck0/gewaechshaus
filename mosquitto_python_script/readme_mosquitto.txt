sudo apt install mosquitto
sudo apt install mosquitto-clients
sudo apt install python3
sudo apt install python3-paho-mqtt

zum testen im Terminal: 

mosquitto -h                                        //starte den Broker
mosquitto_sub -h [BROKER_ADRESS] -t [TOPIC]         //subscribe
