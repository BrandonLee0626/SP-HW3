client: client.c
	gcc client.c cjson/cJSON.c -I./cjson -o client