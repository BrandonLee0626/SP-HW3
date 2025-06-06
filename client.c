// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "cjson/cJSON.h"
#include "board.h"

#define BUF_SIZE 4096

// 전역 사용자명 버퍼
char g_username[32];

#define SIZE 8

int map_char_int(char c)
{
    switch (c)
    {
    case '.': return 0;
    case '#': return 1;
    case 'R': return 2;
    case 'B': return 3;
    }

    return -1;
}

void get_board(cJSON* board_array, int int_board[][10])
{
    for(int i=1;i<=8;i++){
        cJSON* row = cJSON_GetArrayItem(board_array, i-1);
        const char* line = row -> valuestring;
        for(int j=1;j<=8;j++){
            int_board[i][j] = map_char_int(line[j-1]);
        }
    }
}

// 2차원 배열로 보드 복사 (8×8 char)
// JSON 배열을 받아서 board[8][8]에 저장
// board_json은 길이 8인 문자열 배열: 각 문자열은 8글자(R, B, . 중 하나)
static void parse_board(const cJSON *board_json, char board[SIZE][SIZE]) {
    for (int i = 0; i < SIZE; i++) {
        const cJSON *row = cJSON_GetArrayItem(board_json, i);
        const char *str = row->valuestring;
        for (int j = 0; j < SIZE; j++) {
            board[i][j] = str[j];
        }
    }
}
// 8방향 델타

static const int dr[8] = { -1, -1, -1,  0, 1, 1, 1,  0 };

static const int dc[8] = { -1,  0,  1,  1, 1, 0, -1, -1 };



// 보드 경계 검사

static int boundary_check(int r, int c) {

    return (r >= 0 && r < SIZE && c >= 0 && c < SIZE);

}



// board2에 board1 내용 복사

static void copy_board(char dest[SIZE][SIZE], char src[SIZE][SIZE]) {

    memcpy(dest, src, SIZE * SIZE * sizeof(char));

}




static void apply_move(char board[SIZE][SIZE], int r1, int c1, int r2, int c2, char player) {

    int drc = abs(r2 - r1), dcc = abs(c2 - c1);

    if (drc <= 1 && dcc <= 1) {


        board[r2][c2] = player;

    } else {


        board[r1][c1] = '.';

        board[r2][c2] = player;

    }


    char opp = (player == 'R') ? 'B' : 'R';

    for (int f = 0; f < 8; f++) {

        int nr = r2 + dr[f], nc = c2 + dc[f];

        if (boundary_check(nr, nc) && board[nr][nc] == opp) {

            board[nr][nc] = player;

        }

    }

}




static int count_pieces(char board[SIZE][SIZE], char player) {

    int cnt = 0;

    for (int i = 0; i < SIZE; i++)

        for (int j = 0; j < SIZE; j++)

            if (board[i][j] == player) cnt++;

    return cnt;

}




static int is_valid_move(char board[SIZE][SIZE], int r1, int c1, int r2, int c2, char player) {

    if (!boundary_check(r1, c1) || !boundary_check(r2, c2)) return 0;

    if (board[r1][c1] != player) return 0;

    if (board[r2][c2] != '.') return 0;

    int drc = abs(r2 - r1), dcc = abs(c2 - c1);

    int clone = (drc <= 1 && dcc <= 1 && !(drc == 0 && dcc == 0));

    int jump  = ((drc == 2 && dcc == 0) || (drc == 0 && dcc == 2) || (drc == 2 && dcc == 2));

    return (clone || jump);

}




static int gather_moves(char board[SIZE][SIZE], char player, int moves[][4]) {

    int cnt = 0;

    for (int r1 = 0; r1 < SIZE; r1++) {

        for (int c1 = 0; c1 < SIZE; c1++) {

            if (board[r1][c1] != player) continue;

            for (int drd = -2; drd <= 2; drd++) {

                for (int dcd = -2; dcd <= 2; dcd++) {

                    int r2 = r1 + drd, c2 = c1 + dcd;

                    if (is_valid_move(board, r1, c1, r2, c2, player)) {

                        moves[cnt][0] = r1;

                        moves[cnt][1] = c1;

                        moves[cnt][2] = r2;

                        moves[cnt][3] = c2;

                        cnt++;

                    }

                }

            }

        }

    }

    return cnt;

}




static int calc_greedy_value(char board[SIZE][SIZE], int r1, int c1, int r2, int c2, char player) {

    char sim[SIZE][SIZE];

    copy_board(sim, board);

    int before = count_pieces(sim, player);

    apply_move(sim, r1, c1, r2, c2, player);

    int after = count_pieces(sim, player);

    return after - before;

}



static int is_safe_jump(char board[SIZE][SIZE], int r1, int c1, int r2, int c2, char player) {

    int drc = abs(r2 - r1), dcc = abs(c2 - c1);

    int jump = ((drc == 2 && dcc == 0) || (drc == 0 && dcc == 2) || (drc == 2 && dcc == 2));

    if (!jump) return 0;

    char sim[SIZE][SIZE];

    copy_board(sim, board);

    sim[r1][c1] = '.'; 

    char opp = (player == 'R') ? 'B' : 'R';


    for (int rr = 0; rr < SIZE; rr++) {

        for (int cc = 0; cc < SIZE; cc++) {

            if (sim[rr][cc] != opp) continue;

            int drd = abs(r1 - rr), dcd = abs(c1 - cc);

            if ((drd <= 1 && dcd <= 1 && !(drd == 0 && dcd == 0)) || 

                ((drd == 2 && dcd == 0) || (drd == 0 && dcd == 2) || (drd == 2 && dcd == 2))) {

                return 0;  

            }

        }

    }

    return 1;

}



// (r2,c2) 주변 8방향에 있는 내 말 개수 + 모서리/꼭짓점 보너스

static int calc_friend_count(char board[SIZE][SIZE], int r2, int c2, char player) {

    int cnt = 0;

    for (int f = 0; f < 8; f++) {

        int nr = r2 + dr[f], nc = c2 + dc[f];

        if (boundary_check(nr, nc) && board[nr][nc] == player) {

            cnt++;

        }

    }

    int edge = (r2 == 0 || r2 == SIZE - 1) + (c2 == 0 || c2 == SIZE - 1);

    if (edge == 2)      cnt += 3; 

    else if (edge == 1) cnt += 1; 

    return cnt;

}




extern bool move_generate_greedy_2n(char b[SIZE][SIZE], char player,

                                    int *out_r1, int *out_c1,

                                    int *out_r2, int *out_c2);




static int evaluate_five_greedy(char board[SIZE][SIZE],

                                int r1, int c1, int r2, int c2, char me) {

    char opp = (me == 'R') ? 'B' : 'R';





    int myGV1 = calc_greedy_value(board, r1, c1, r2, c2, me);




    char board1[SIZE][SIZE];

    copy_board(board1, board);

    apply_move(board1, r1, c1, r2, c2, me);




    int opp_moves1[SIZE*SIZE*8][4];

    int opp_cnt1 = gather_moves(board1, opp, opp_moves1);

    int bestOppGV1 = 0;

    int or11 = -1, oc11 = -1, or12 = -1, oc12 = -1;

    for (int i = 0; i < opp_cnt1; i++) {

        int rr1 = opp_moves1[i][0], cc1 = opp_moves1[i][1];

        int rr2 = opp_moves1[i][2], cc2 = opp_moves1[i][3];

        int g = calc_greedy_value(board1, rr1, cc1, rr2, cc2, opp);

        if (i == 0 || g > bestOppGV1) {

            bestOppGV1 = g;

            or11 = rr1;  oc11 = cc1;

            or12 = rr2;  oc12 = cc2;

        }

    }


    char board2[SIZE][SIZE];

    if (opp_cnt1 > 0) {

        copy_board(board2, board1);

        apply_move(board2, or11, oc11, or12, oc12, opp);

    } else {

        copy_board(board2, board1);

    }




    int my_moves2[SIZE*SIZE*8][4];

    int my_cnt2 = gather_moves(board2, me, my_moves2);

    int bestMyGV2 = 0;

    int mr21 = -1, mc21 = -1, mr22 = -1, mc22 = -1;

    for (int i = 0; i < my_cnt2; i++) {

        int rr1 = my_moves2[i][0], cc1 = my_moves2[i][1];

        int rr2 = my_moves2[i][2], cc2 = my_moves2[i][3];

        int g = calc_greedy_value(board2, rr1, cc1, rr2, cc2, me);

        if (i == 0 || g > bestMyGV2) {

            bestMyGV2 = g;

            mr21 = rr1;  mc21 = cc1;

            mr22 = rr2;  mc22 = cc2;

        }

    }


    char board3[SIZE][SIZE];

    if (my_cnt2 > 0) {

        copy_board(board3, board2);

        apply_move(board3, mr21, mc21, mr22, mc22, me);

    } else {

        copy_board(board3, board2);

    }




    int opp_moves2[SIZE*SIZE*8][4];

    int opp_cnt2 = gather_moves(board3, opp, opp_moves2);

    int bestOppGV2 = 0;

    int or21 = -1, oc21 = -1, or22 = -1, oc22 = -1;

    for (int i = 0; i < opp_cnt2; i++) {

        int rr1 = opp_moves2[i][0], cc1 = opp_moves2[i][1];

        int rr2 = opp_moves2[i][2], cc2 = opp_moves2[i][3];

        int g = calc_greedy_value(board3, rr1, cc1, rr2, cc2, opp);

        if (i == 0 || g > bestOppGV2) {

            bestOppGV2 = g;

            or21 = rr1;  oc21 = cc1;

            or22 = rr2;  oc22 = cc2;

        }

    }


    char board4[SIZE][SIZE];

    if (opp_cnt2 > 0) {

        copy_board(board4, board3);

        apply_move(board4, or21, oc21, or22, oc22, opp);

    } else {

        copy_board(board4, board3);

    }




    int my_moves3[SIZE*SIZE*8][4];

    int my_cnt3 = gather_moves(board4, me, my_moves3);

    int bestMyGV3 = 0;

    for (int i = 0; i < my_cnt3; i++) {

        int rr1 = my_moves3[i][0], cc1 = my_moves3[i][1];

        int rr2 = my_moves3[i][2], cc2 = my_moves3[i][3];

        int g = calc_greedy_value(board4, rr1, cc1, rr2, cc2, me);

        if (i == 0 || g > bestMyGV3) {

            bestMyGV3 = g;

        }

    }



    return (myGV1 - bestOppGV1 + bestMyGV2 - bestOppGV2 + bestMyGV3);

}




void generate_move(const cJSON *board_json, int *sx, int *sy, int *tx, int *ty, char me) {

    char board[SIZE][SIZE];

    parse_board(board_json, board);




    int empty_cnt = 0;

    for (int i = 0; i < SIZE; i++) {

        for (int j = 0; j < SIZE; j++) {

            if (board[i][j] == '.') empty_cnt++;

        }

    }

    bool last_one = (empty_cnt == 1);




    int moves[SIZE*SIZE*8][4];

    int n_moves = gather_moves(board, me, moves);

    if (n_moves == 0) {

        *sx = *sy = *tx = *ty = 0;

        return;

    }



    int bestEval   = -1000000;

    int bestType   =  3;

    int bestFriend = -1;

    int bestR1=0, bestC1=0, bestR2=0, bestC2=0;



    for (int i = 0; i < n_moves; i++) {

        int r1 = moves[i][0], c1 = moves[i][1];

        int r2 = moves[i][2], c2 = moves[i][3];



        int drc = abs(r2 - r1), dcc = abs(c2 - c1);

        bool is_jump = (drc == 2 || dcc == 2);




        if (last_one && is_jump) continue;



        int eval = evaluate_five_greedy(board, r1, c1, r2, c2, me);



        bool safe = is_safe_jump(board, r1, c1, r2, c2, me);

        int type;

        if (is_jump && safe) type = 0;

        else if (!is_jump)   type = 1;

        else                 type = 2;



        char sim[SIZE][SIZE];

        copy_board(sim, board);

        apply_move(sim, r1, c1, r2, c2, me);

        int friendCnt = calc_friend_count(sim, r2, c2, me);



        bool better = false;

        if (eval > bestEval) {

            better = true;

        } else if (eval == bestEval) {

            if (type < bestType) {

                better = true;

            } else if (type == bestType) {

                if (friendCnt > bestFriend) {

                    better = true;

                } else if (friendCnt == bestFriend) {

                    if (r2 < bestR2 || (r2 == bestR2 && c2 < bestC2)) {

                        better = true;

                    }

                }

            }

        }



        if (better) {

            bestEval   = eval;

            bestType   = type;

            bestFriend = friendCnt;

            bestR1 = r1; bestC1 = c1;

            bestR2 = r2; bestC2 = c2;

        }

    }



    *sx = bestR1 + 1;

    *sy = bestC1 + 1;

    *tx = bestR2 + 1;

    *ty = bestC2 + 1;

}




static char *recv_json(int fd) {
    char buffer[BUF_SIZE];
    int idx = 0;
    while (1) {
        int n = recv(fd, buffer + idx, 1, 0);
        if (n <= 0) {
            return NULL;  
        }
        if (buffer[idx] == '\n') {
            buffer[idx] = '\0';
            break;
        }
        idx++;
        if (idx >= BUF_SIZE - 1) {
            return NULL; 
        }
    }
    return strdup(buffer);
}

static int send_json(int fd, cJSON *obj) {
    char *json_str = cJSON_PrintUnformatted(obj);
    if (!json_str) return -1;
    int len = snprintf(NULL, 0, "%s\n", json_str);
    char *buf = (char *)malloc(len + 1);
    if (!buf) {
        free(json_str);
        return -1;
    }
    snprintf(buf, len + 1, "%s\n", json_str);
    int sent = send(fd, buf, strlen(buf), 0);
    free(buf);
    free(json_str);
    return sent;
}

static void print_usage(const char *progname) {
    fprintf(stderr,
            "Usage: %s -ip <server_ip> -port <server_port> -username <your_username>\n"
            "Example:\n"
            "  %s -ip 10.8.128.233 -port 8080 -username Moonyoung\n",
            progname, progname);
}

#ifdef CLIENT_STANDALONE
int main(int argc, char *argv[]) {
    RGBLedMatrixOptions options;
    memset(&options, 0, sizeof(options));
    options.rows = 64;
    options.cols = 64;
    options.chain_length = 1;
    options.parallel = 1;
    options.hardware_mapping = "regular";
    options.brightness = 50;
    options.disable_hardware_pulsing = 1;

    char me;

    struct RGBLedMatrix *matrix = led_matrix_create_from_options(&options, NULL, NULL);
    if (matrix == NULL) {
        return 1;
    }

    struct LedCanvas *canvas = led_matrix_get_canvas(matrix);

    int board[10][10];
    float N, r1_, c1_, r2_, c2_, temp;
    int pass_flag=0;

    if (argc != 7) {
        print_usage(argv[0]);
        return 1;
    }

    char server_ip[64] = {0};
    char server_port[16] = {0};
    char username[32]   = {0};

    int int_board[10][10];

    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "-ip") == 0) {
            strncpy(server_ip, argv[i + 1], sizeof(server_ip) - 1);
        }
        else if (strcmp(argv[i], "-port") == 0) {
            strncpy(server_port, argv[i + 1], sizeof(server_port) - 1);
        }
        else if (strcmp(argv[i], "-username") == 0) {
            strncpy(username, argv[i + 1], sizeof(username) - 1);
        }
        else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (server_ip[0] == '\0' || server_port[0] == '\0' || username[0] == '\0') {
        print_usage(argv[0]);
        return 1;
    }

    strncpy(g_username, username, sizeof(g_username) - 1);
    g_username[sizeof(g_username) - 1] = '\0';

    struct addrinfo hints, *res;
    int sockfd, status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((status = getaddrinfo(server_ip, server_port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo 오류: %s\n", gai_strerror(status));
        return 1;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("socket 생성 오류");
        freeaddrinfo(res);
        return 1;
    }
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("connect 실패");
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }
    freeaddrinfo(res);

    cJSON *reg = cJSON_CreateObject();
    cJSON_AddStringToObject(reg, "type", "register");
    cJSON_AddStringToObject(reg, "username", g_username);
    send_json(sockfd, reg);
    cJSON_Delete(reg);
    printf("[클라이언트] register 전송: %s\n", g_username);

    while (1) {
        char *msg = recv_json(sockfd);
        if (!msg) {
            printf("서버 연결 종료 또는 수신 실패\n");
            break;
        }

        cJSON *root = cJSON_Parse(msg);
        free(msg);
        if (!root) {
            printf("JSON 파싱 실패\n");
            continue;
        }

        cJSON *type = cJSON_GetObjectItem(root, "type");
        if (!cJSON_IsString(type)) {
            cJSON_Delete(root);
            continue;
        }

        if (strcmp(type->valuestring, "register_ack") == 0) {
            printf("[서버] register_ack 수신\n");
        }

        else if (strcmp(type->valuestring, "game_start") == 0) {
            cJSON* first_player = cJSON_GetObjectItemCaseSensitive(root, "first_player");
            if(strcmp(first_player->valuestring, g_username) == 0) me = 'R';
            else me = 'B';
            printf("%c\n", me);
            printf("[서버] game_start 수신\n");
        }

        else if (strcmp(type->valuestring, "your_turn") == 0) {
            printf("[서버] your_turn 수신\n");
            cJSON *board_json = cJSON_GetObjectItem(root, "board");
            cJSON *timeout    = cJSON_GetObjectItem(root, "timeout");

            get_board(board_json, int_board);
            clear_board(canvas);
            draw_board(canvas, int_board);

            usleep(100000);

            if (cJSON_IsArray(board_json) && cJSON_IsNumber(timeout)) {
                int sx = 0, sy = 0, tx = 0, ty = 0;

                generate_move(board_json, &sx, &sy, &tx, &ty, me);

                cJSON *mv = cJSON_CreateObject();
                cJSON_AddStringToObject(mv, "type", "move");
                cJSON_AddStringToObject(mv, "username", g_username);
                cJSON_AddNumberToObject(mv, "sx", sx);
                cJSON_AddNumberToObject(mv, "sy", sy);
                cJSON_AddNumberToObject(mv, "tx", tx);
                cJSON_AddNumberToObject(mv, "ty", ty);
                send_json(sockfd, mv);
                cJSON_Delete(mv);
                printf("[클라이언트] move 전송: (%d,%d) -> (%d,%d)\n", sx, sy, tx, ty);
            }
        }

        else if (strcmp(type->valuestring, "move_ok") == 0) {
            printf("[서버] move_ok 수신\n");
            cJSON *board_arr = cJSON_GetObjectItem(root, "board");
            if (cJSON_IsArray(board_arr)) {
                printf("----- 현재 보드 상태 (move_ok) -----\n");
                int rows = cJSON_GetArraySize(board_arr);
                for (int i = 0; i < rows; i++) {
                    cJSON *row = cJSON_GetArrayItem(board_arr, i);
                    if (cJSON_IsString(row)) {
                        printf("%s\n", row->valuestring);
                    }
                }
                printf("-----------------------------------\n");
            }
        }

        else if (strcmp(type->valuestring, "invalid_move") == 0) {
            printf("[서버] invalid_move 수신: 잘못된 수\n");
        }

        else if (strcmp(type->valuestring, "pass") == 0) {
            cJSON *uname = cJSON_GetObjectItem(root, "username");
            cJSON *nextp = cJSON_GetObjectItem(root, "next_player");
            if (cJSON_IsString(uname) && cJSON_IsString(nextp)) {
                printf("[서버] %s 패스 → 다음 턴: %s\n", uname->valuestring, nextp->valuestring);
            }
        }

        else if (strcmp(type->valuestring, "game_over") == 0) {
            printf("[서버] game_over 수신\n");

            cJSON *board_arr = cJSON_GetObjectItem(root, "board");
            if (cJSON_IsArray(board_arr)) {
                printf("----- 최종 보드 상태 (game_over) -----\n");
                int rows = cJSON_GetArraySize(board_arr);
                for (int i = 0; i < rows; i++) {
                    cJSON *row = cJSON_GetArrayItem(board_arr, i);
                    if (cJSON_IsString(row)) {
                        printf("%s\n", row->valuestring);
                    }
                }
                printf("------------------------------------\n");
            }

            cJSON *scores = cJSON_GetObjectItem(root, "scores");
            if (scores && cJSON_IsObject(scores)) {
                cJSON *me = cJSON_GetObjectItem(scores, g_username);
                printf("최종 점수 - %s: %d\n", g_username, me ? me->valueint : 0);
                cJSON *tmp = NULL;
                cJSON_ArrayForEach(tmp, scores) {
                    if (strcmp(tmp->string, g_username) != 0) {
                        printf("상대(%s) 점수: %d\n", tmp->string, tmp->valueint);
                    }
                }
            }
            cJSON_Delete(root);
            break;  
        }

        else {
            printf("[서버] 알 수 없는 메시지 유형: %s\n", type->valuestring);
        }

        cJSON_Delete(root);
    }

    close(sockfd);
    printf("클라이언트 종료\n");
    return 0;
}
#endif