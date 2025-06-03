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
static void copy_board(char dest[SIZE][SIZE], char src[SIZE][SIZE]) {
    memcpy(dest, src, SIZE * SIZE);
}

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

// r1,c1 → r2,c2 이동이 유효한지 검사 (clone/jump 규칙)
// player는 'R' 또는 'B'
// 빈칸('.')인지, 내 말인지, 일정 거리 조건(≤1 or ==2)인지 확인
static int is_valid_move(char board[SIZE][SIZE], int r1, int c1, int r2, int c2, char player) {
    if (r1 < 0 || r1 >= SIZE || c1 < 0 || c1 >= SIZE) return 0;
    if (r2 < 0 || r2 >= SIZE || c2 < 0 || c2 >= SIZE) return 0;
    if (board[r1][c1] != player) return 0;
    if (board[r2][c2] != '.') return 0;
    int dr = abs(r2 - r1), dc = abs(c2 - c1);
    int clone = (dr <= 1 && dc <= 1 && !(dr == 0 && dc == 0));
    int jump  = ((dr == 2 && dc == 0) || (dr == 0 && dc == 2) || (dr == 2 && dc == 2));
    return clone || jump;
}

// 이동 적용: clone 또는 jump 후 flip 처리
static void apply_move(char board[SIZE][SIZE], int r1, int c1, int r2, int c2, char player) {
    int dr = abs(r2 - r1), dc = abs(c2 - c1);
    if (dr <= 1 && dc <= 1) {
        // clone: 출발지에 말은 그대로, 목적지에 새로운 내 말
        board[r2][c2] = player;
    } else {
        // jump: 출발지를 비우고 목적지에 내 말
        board[r1][c1] = '.';
        board[r2][c2] = player;
    }
    // flip: 목적지(r2,c2)를 중심으로 8방향에 상대 말이 있으면 내 말로 뒤집기
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dy == 0 && dx == 0) continue;
            int nr = r2 + dy, nc = c2 + dx;
            if (nr < 0 || nr >= SIZE || nc < 0 || nc >= SIZE) continue;
            if (board[nr][nc] != player && board[nr][nc] != '.') {
                board[nr][nc] = player;
            }
        }
    }
}

// board 상에서 특정 색(player)의 말 개수 세기
static int count_pieces(char board[SIZE][SIZE], char player) {
    int cnt = 0;
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            if (board[i][j] == player) cnt++;
    return cnt;
}

// 주어진 시뮬레이트 보드에서, player_other가 (sr,sc)로 이동 가능한지 검사
// (clone/jump 조건만 보면 되는지 확인) → safe jump 판단에 사용
static int can_opponent_move_to(char board[SIZE][SIZE], int sr, int sc, char player_other) {
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] != player_other) continue;
            int dr = abs(sr - r), dc = abs(sc - c);
            // clone으로 들어올 수 있는지
            if ((dr <= 1 && dc <= 1) && !(dr == 0 && dc == 0)) {
                return 1;
            }
            // jump으로 들어올 수 있는지
            if (( (dr == 2 && dc == 0) || (dr == 0 && dc == 2) || (dr == 2 && dc == 2) )) {
                return 1;
            }
        }
    }
    return 0;
}

// (r1,c1)->(r2,c2) 이동 후보에 대해 “내 그리디 값” 계산
// 이동 전/후 내 말 개수 차이 = after - before
static int calc_greedy_value(char board[SIZE][SIZE], int r1, int c1, int r2, int c2, char player) {
    char sim[SIZE][SIZE];
    copy_board(sim, board);
    int before = count_pieces(sim, player);
    apply_move(sim, r1, c1, r2, c2, player);
    int after = count_pieces(sim, player);
    return after - before;
}

// 주어진 보드에서, player의 모든 유효 이동을 (r1,c1,r2,c2) 형태로 moves[][4]에 채우고 개수 반환
static int gather_moves(char board[SIZE][SIZE], char player, int moves[][4]) {
    int cnt = 0;
    for (int r1 = 0; r1 < SIZE; r1++) {
        for (int c1 = 0; c1 < SIZE; c1++) {
            if (board[r1][c1] != player) continue;
            for (int dr = -2; dr <= 2; dr++) {
                for (int dc = -2; dc <= 2; dc++) {
                    int r2 = r1 + dr, c2 = c1 + dc;
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

// 시뮬 보드에 (r1,c1)->(r2,c2,player) 이동 적용 후
// 상대(opp)가 가질 수 있는 모든 이동의 그리디 값을 계산해서 그중 최대값 반환
static int calc_opponent_max_greedy(char board[SIZE][SIZE], int r1, int c1, int r2, int c2, char player) {
    char sim[SIZE][SIZE];
    copy_board(sim, board);
    apply_move(sim, r1, c1, r2, c2, player);
    char opp = (player == 'R') ? 'B' : 'R';

    int opp_moves[SIZE*SIZE*8][4];
    int opp_cnt = gather_moves(sim, opp, opp_moves);
    int max_gv = 0;
    for (int i = 0; i < opp_cnt; i++) {
        int or1 = opp_moves[i][0], oc1 = opp_moves[i][1];
        int or2 = opp_moves[i][2], oc2 = opp_moves[i][3];
        int gv = calc_greedy_value(sim, or1, oc1, or2, oc2, opp);
        if (gv > max_gv) max_gv = gv;
    }
    return max_gv;
}

// (r1,c1)->(r2,c2)이 점프인지 검사하고, safe jump(안전한 점프)인지 여부 반환
// safe jump: 내가 점프해서 출발지 칸이 비워진 뒤, 상대가 그 출발지로 이동할 수 없으면 안전
static int is_safe_jump(char board[SIZE][SIZE], int r1, int c1, int r2, int c2, char player) {
    int dr = abs(r2 - r1), dc = abs(c2 - c1);
    int jump = ((dr == 2 && dc == 0) || (dr == 0 && dc == 2) || (dr == 2 && dc == 2));
    if (!jump) return 0; // clone은 점프가 아님

    char sim[SIZE][SIZE];
    copy_board(sim, board);
    // 점프하면 출발지(r1,c1)는 빈칸이 됨
    sim[r1][c1] = '.';
    char opp = (player == 'R') ? 'B' : 'R';
    if (can_opponent_move_to(sim, r1, c1, opp)) return 0; // 상대가 들어올 수 있으면 unsafe
    return 1; // 들어올 수 없으면 safe
}

// 목적지(r2,c2) 주변 8칸에 내 돌이 몇 개 있나 세고, 
// 만약 모서리(가장자리)이면 +3, 꼭짓점(4개 코너)이면 +4 보너스 붙여 반환
static int calc_friend_count(char board[SIZE][SIZE], int r2, int c2, char player) {
    int cnt = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dy == 0 && dx == 0) continue;
            int nr = r2 + dy, nc = c2 + dx;
            if (nr < 0 || nr >= SIZE || nc < 0 || nc >= SIZE) continue;
            if (board[nr][nc] == player) cnt++;
        }
    }
    // 모서리/꼭짓점 보너스
    int edge = (r2 == 0 || r2 == SIZE-1) + (c2 == 0 || c2 == SIZE-1);
    if (edge == 2) cnt += 5;   // 꼭짓점
    else if (edge == 1) cnt += 1; // 모서리
    return cnt;
}

void generate_move(const cJSON *board_json, int *sx, int *sy, int *tx, int *ty) {
    char board[SIZE][SIZE];
    parse_board(board_json, board);

    // 여기서는 편의상 “내 말”을 항상 'R'이라고 가정
    // 나중에 클라이언트 쪽에서 첫 플레이어인지 판별해 'B'로 바꿔 주어야 함
    char me = 'R';

    // 1) 내 모든 유효 이동 후보 수집
    int moves[SIZE*SIZE*8][4];
    int n_moves = gather_moves(board, me, moves);
    if (n_moves == 0) {
        // 이동 불가 → 패스
        *sx = *sy = *tx = *ty = 0;
        return;
    }

    // 최종 후보를 담을 변수
    int bestEval   = -1000000;
    int bestType   =  3;    // type: 0=safe jump, 1=clone, 2=unsafe jump
    int bestFriend = -1;    // 친구 수
    int bestR1=0, bestC1=0, bestR2=0, bestC2=0;

    // 2) 각 후보 이동마다 “Eval” 계산 → (내 그리디) – (상대 최대 그리디)
    for (int i = 0; i < n_moves; i++) {
        int r1 = moves[i][0], c1 = moves[i][1];
        int r2 = moves[i][2], c2 = moves[i][3];

        int myGV  = calc_greedy_value(board, r1, c1, r2, c2, me);
        int oppGV = calc_opponent_max_greedy(board, r1, c1, r2, c2, me);
        int eval  = myGV - oppGV;

        // 이동 유형 판별: jump/clone, 그리고 safe jump 여부
        int dr = abs(r2 - r1), dc = abs(c2 - c1);
        int jump = ((dr == 2 && dc == 0) || (dr == 0 && dc == 2) || (dr == 2 && dc == 2));
        int safe = is_safe_jump(board, r1, c1, r2, c2, me);
        int type;
        if (jump && safe) type = 0;      // 안전한 점프
        else if (!jump)  type = 1;       // 클론
        else              type = 2;      // 불안전한 점프

        // “친구 수” 계산을 위해 시뮬 보드에 한 번 적용
        char sim[SIZE][SIZE];
        copy_board(sim, board);
        apply_move(sim, r1, c1, r2, c2, me);
        int friendCnt = calc_friend_count(sim, r2, c2, me);

        // 3단계 우선순위 비교: Eval → type → friendCnt → 좌표(행,열)
        int better = 0;
        if (eval > bestEval) better = 1;
        else if (eval == bestEval) {
            if (type < bestType) better = 1;
            else if (type == bestType) {
                if (friendCnt > bestFriend) better = 1;
                else if (friendCnt == bestFriend) {
                    // 좌표 비교: r2(행) 오름차순, 같으면 c2(열) 오름차순
                    if (r2 < bestR2 || (r2 == bestR2 && c2 < bestC2)) better = 1;
                }
            }
        }

        if (better) {
            bestEval   = eval;
            bestType   = type;
            bestFriend = friendCnt;
            bestR1 = r1;    bestC1 = c1;
            bestR2 = r2;    bestC2 = c2;
        }
    }

    // 최종 선택된 이동을 1-based 인덱스로 리턴
    *sx = bestR1 + 1;
    *sy = bestC1 + 1;
    *tx = bestR2 + 1;
    *ty = bestC2 + 1;
}

// recv_json: fd로부터 '\n' 단위로 JSON 문자열 한 줄을 읽어서 동적 할당된 문자열로 반환
static char *recv_json(int fd) {
    char buffer[BUF_SIZE];
    int idx = 0;
    while (1) {
        int n = recv(fd, buffer + idx, 1, 0);
        if (n <= 0) {
            return NULL;  // 오류 또는 연결 종료
        }
        if (buffer[idx] == '\n') {
            buffer[idx] = '\0';
            break;
        }
        idx++;
        if (idx >= BUF_SIZE - 1) {
            return NULL;  // 버퍼 초과
        }
    }
    return strdup(buffer);
}

// send_json: cJSON 객체를 직렬화하여 fd로 전송 (뒤에 '\n' 추가)
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

int main(int argc, char *argv[]) {
    RGBLedMatrixOptions options;
    memset(&options, 0, sizeof(options));
    options.rows = 64;
    options.cols = 64;
    options.chain_length = 1;
    options.parallel = 1;
    options.hardware_mapping = "regular";
    options.brightness = 100;
    options.disable_hardware_pulsing = 1;

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

    // 플래그 파싱 (-ip, -port, -username)
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

    // 전역 버퍼에 username 복사
    strncpy(g_username, username, sizeof(g_username) - 1);
    g_username[sizeof(g_username) - 1] = '\0';

    struct addrinfo hints, *res;
    int sockfd, status;

    // 1) 서버 주소 해석
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((status = getaddrinfo(server_ip, server_port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo 오류: %s\n", gai_strerror(status));
        return 1;
    }

    // 2) 소켓 생성 및 서버 연결
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

    // 3) register 메시지 전송
    cJSON *reg = cJSON_CreateObject();
    cJSON_AddStringToObject(reg, "type", "register");
    cJSON_AddStringToObject(reg, "username", g_username);
    send_json(sockfd, reg);
    cJSON_Delete(reg);
    printf("[클라이언트] register 전송: %s\n", g_username);

    // 4) 서버 메시지 수신 루프
    while (1) {
        clear_board(canvas);
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

        //──────────────────────────────────────────────────────────────────────
        // register_ack
        //──────────────────────────────────────────────────────────────────────
        if (strcmp(type->valuestring, "register_ack") == 0) {
            printf("[서버] register_ack 수신\n");
        }
        //──────────────────────────────────────────────────────────────────────
        // game_start
        //──────────────────────────────────────────────────────────────────────
        else if (strcmp(type->valuestring, "game_start") == 0) {
            printf("[서버] game_start 수신\n");
            // 첫 플레이어 정보는 board.c 내부 로직에서 필요 시 사용
        }
        //──────────────────────────────────────────────────────────────────────
        // your_turn
        //──────────────────────────────────────────────────────────────────────
        else if (strcmp(type->valuestring, "your_turn") == 0) {
            printf("[서버] your_turn 수신\n");
            cJSON *board_json = cJSON_GetObjectItem(root, "board");
            cJSON *timeout    = cJSON_GetObjectItem(root, "timeout");

            get_board(board_json, int_board);
            draw_board(canvas, int_board);

            sleep(1);

            if (cJSON_IsArray(board_json) && cJSON_IsNumber(timeout)) {
                int sx = 0, sy = 0, tx = 0, ty = 0;

                // generate_move 함수 호출: 보드 상태에서 최적 수 계산
                generate_move(board_json, &sx, &sy, &tx, &ty);

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
        //──────────────────────────────────────────────────────────────────────
        // move_ok: 보드 출력
        //──────────────────────────────────────────────────────────────────────
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
        //──────────────────────────────────────────────────────────────────────
        // invalid_move
        //──────────────────────────────────────────────────────────────────────
        else if (strcmp(type->valuestring, "invalid_move") == 0) {
            printf("[서버] invalid_move 수신: 잘못된 수\n");
        }
        //──────────────────────────────────────────────────────────────────────
        // pass
        //──────────────────────────────────────────────────────────────────────
        else if (strcmp(type->valuestring, "pass") == 0) {
            cJSON *uname = cJSON_GetObjectItem(root, "username");
            cJSON *nextp = cJSON_GetObjectItem(root, "next_player");
            if (cJSON_IsString(uname) && cJSON_IsString(nextp)) {
                printf("[서버] %s 패스 → 다음 턴: %s\n", uname->valuestring, nextp->valuestring);
            }
        }
        //──────────────────────────────────────────────────────────────────────
        // game_over: 최종 점수 출력 후 종료
        //──────────────────────────────────────────────────────────────────────
        else if (strcmp(type->valuestring, "game_over") == 0) {
            printf("[서버] game_over 수신\n");

            // 1) 보드(JSON 배열) 파싱해서 줄 단위로 출력 (서버에서 board 필드 추가된 경우)
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

            // 2) 점수 출력
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
            break;  // game_over이므로 루프 탈출
        }
        //──────────────────────────────────────────────────────────────────────
        // 그 외 알 수 없는 메시지
        //──────────────────────────────────────────────────────────────────────
        else {
            printf("[서버] 알 수 없는 메시지 유형: %s\n", type->valuestring);
        }

        cJSON_Delete(root);
    }

    close(sockfd);
    printf("클라이언트 종료\n");
    return 0;
}