all: client board

client: client.c
	g++ -DCLIENT_STANDALONE client.c board.c cjson/cJSON.c -o client \
	-I./cjson -I./rpi-rgb-led-matrix/include \
	-L./rpi-rgb-led-matrix/lib -lrgbmatrix -lpthread -lrt

board: board.c
	g++ -DBOARD_STANDALONE board.c -o board -I./rpi-rgb-led-matrix/include -L./rpi-rgb-led-matrix/lib -lrgbmatix -lpthread -lrt

clean:
	rm -f client