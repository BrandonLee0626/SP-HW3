client: client.c
	g++ client.c board.c -o client -I./cjson -I./rpi-rgb-led-matrix/include -L./rpi-rgb-led-matrix/lib -lrgbmatrix -lcjson -lpthread -lrt

clean:
	rm -f client