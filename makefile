client: client.c
	g++ -DCLIENT_STANDALONE client.c board.c -o client \
	-I./cjson \
	-I./rpi-rgb-led-matrix/include \
	-L./rpi-rgb-led-matrix/lib \
	-lrgbmatrix -lcjson -lpthread -lrt

board: board.c
	g++ -DBOARD_STANDALONE board.c -o board \
	-I./rpi-rgb-led-matrix/include \
	-L./rpi-rgb-led-matrix/lib \
	-lrgbmatix -lcjson -lpthread -lrt

clean:
	rm -f client