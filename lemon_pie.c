#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <termios.h>
#include <julius/juliuslib.h>
#include "sensor_cmd.h"

#define LOG_TAG		"[lemon_pie] "
#define JCONF_FILE	"remocon.jconf"
#define SENSOR_PORT_STR	"26852"
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(a[0]))

static struct {
	const char *squash_server;
	int has_sensor;
	int verbose;
} app;

static void clear_callbacks(Recog *recog);

static const char *julius_status_str(int status)
{
	switch (status) {
	case J_RESULT_STATUS_BUFFER_OVERFLOW:
		return "input buffer overflow";
	case J_RESULT_STATUS_REJECT_POWER:
		return "input rejected by power";
	case J_RESULT_STATUS_TERMINATE:
		return "input teminated by request";
	case J_RESULT_STATUS_ONLY_SILENCE:
		return "input rejected by decoder (silence input result)";
	case J_RESULT_STATUS_REJECT_GMM:
		return "input rejected by GMM";
	case J_RESULT_STATUS_REJECT_SHORT:
		return "input rejected by short input";
	case J_RESULT_STATUS_FAIL:
		return "failed";
	case J_RESULT_STATUS_SUCCESS:
		return "success";
	default:
		return "unknown status";
	}
}

struct {
	const char *word;
	const char *lc_cmd;
} cmd_table[] = {
	{ "テレビ電源on",		"tv_pwr_on" },
	{ "テレビ電源off",		"tv_pwr_off" },
	{ "テレビ音量大",		"tv_vol_up" },
	{ "テレビ音量小",		"tv_vol_down" },
	{ "テレビミュート",		"tv_mute" },
	{ "テレビ1ch",			"tv_ch1" },
	{ "テレビ2ch",			"tv_ch2" },
	{ "テレビ3ch",			"tv_ch3" },
	{ "テレビ4ch",			"tv_ch4" },
	{ "テレビ5ch",			"tv_ch5" },
	{ "テレビ6ch",			"tv_ch6" },
	{ "テレビ7ch",			"tv_ch7" },
	{ "テレビ8ch",			"tv_ch8" },
	{ "テレビ9ch",			"tv_ch9" },
	{ "テレビ10ch",			"tv_ch10" },
	{ "テレビ11ch",			"tv_ch11" },
	{ "テレビ12ch",			"tv_ch12" },
	{ "CATV電源",			"catv_pwr" },
	{ "GAORA",			"tv_component1 catv_catv catv_input_ch"
					" catv_4 catv_2 catv_7" },
	{ "G+",				"tv_component1 catv_catv catv_input_ch"
					" catv_4 catv_2 catv_9" },
	{ "J SPORTS 3",			"tv_component1 catv_catv catv_input_ch"
					" catv_4 catv_3 catv_0" },
	{ "J SPORTS 1",			"tv_component1 catv_catv catv_input_ch"
					" catv_4 catv_3 catv_1" },
	{ "J SPORTS 2",			"tv_component1 catv_catv catv_input_ch"
					" catv_4 catv_3 catv_2" },
	{ "ファミ劇",			"tv_component1 catv_catv catv_input_ch"
					" catv_5 catv_5 catv_8" },
	{ "キッズステーション",		"tv_component1 catv_catv catv_input_ch"
					" catv_6 catv_10 catv_1" },
	{ "カートゥーンネットワーク",	"tv_component1 catv_catv catv_input_ch"
					" catv_6 catv_10 catv_2" },
	{ "アニマックス",		"tv_component1 catv_catv catv_input_ch"
					" catv_6 catv_2 catv_5" },
	{ "ナショジオ",			"tv_component1 catv_catv catv_input_ch"
					" catv_6 catv_5 catv_1" },
	{ "ディスカバリーチャンネル",	"tv_component1 catv_catv catv_input_ch"
					" catv_6 catv_7 catv_7" },
};

static void on_speech_ready(Recog *recog, void *dummy)
{
	fprintf(stderr, LOG_TAG "recognition started\n");
}

static void on_speech_start(Recog *recog, void *dummy)
{
	fprintf(stderr, LOG_TAG "detecting...\n");
}

static void dispatch(const char *lc_cmd)
{
	char cmd[256];

	if (app.squash_server)
		sprintf(cmd, "lemon_corn -proxy %s %s",
			app.squash_server, lc_cmd);
	else
		sprintf(cmd, "lemon_corn %s", lc_cmd);
	system(cmd);
}

static void on_result(Recog *recog, void *dummy)
{
	/* we have only one recognition process */
	RecogProcess *rp = recog->process_list;
	WORD_INFO *winfo;
	int i;

	if (rp->result.status < 0) {
		/* no results obtained */
		fprintf(stderr, LOG_TAG "result error: %s\n",
			julius_status_str(rp->result.status));
		return;
	}

	winfo = rp->lm->winfo;

	for (i = 0; i < rp->result.sentnum; i++) {
		Sentence *snt = &(rp->result.sent[i]);
		int word_cnt = snt->word_num;
		int j;

		/* result words */
		for (j = 0; j < word_cnt; j++) {
			const char *wstr = winfo->woutput[snt->word[j]];
			printf(" %s", wstr);
		}
		putchar('\n');

		/* action */
		for (j = 0; j < word_cnt; j++) {
			unsigned int k;
			const char *wstr = winfo->woutput[snt->word[j]];

			for (k = 0; k < ARRAY_SIZE(cmd_table); k++) {
				if (!strcmp(wstr, cmd_table[k].word))
					dispatch(cmd_table[k].lc_cmd);
			}
		}

		/* score */
		if (app.verbose)
			printf("score%d: %f\n", i + 1, snt->score);
	}

	j_close_stream(recog);
	clear_callbacks(recog);
}

static struct {
	int id_ready;
	int id_start;
	int id_result;
} g_cbs;

static int add_callbacks(Recog *recog)
{
	g_cbs.id_ready = callback_add(recog, CALLBACK_EVENT_SPEECH_READY,
				      on_speech_ready, NULL);
	if (g_cbs.id_ready < 0) {
		fprintf(stderr,
			"failed to register callback on_speech_ready\n");
		return -1;
	}
	g_cbs.id_start = callback_add(recog, CALLBACK_EVENT_SPEECH_START,
				      on_speech_start, NULL);
	if (g_cbs.id_ready < 0) {
		fprintf(stderr,
			"failed to register callback on_speech_start\n");
		return -1;
	}
	g_cbs.id_result = callback_add(recog, CALLBACK_RESULT, on_result, NULL);
	if (g_cbs.id_ready < 0) {
		fprintf(stderr, "failed to register callback on_result\n");
		return -1;
	}

	return 0;
}

static void clear_callbacks(Recog *recog)
{
	callback_delete(recog, g_cbs.id_ready);
	callback_delete(recog, g_cbs.id_start);
	callback_delete(recog, g_cbs.id_result);
}

/*
 * return value:
 *   1: continue
 *   0: quit
 *  -1: error
 */
static int main_loop(int fd_sensor, Recog *recog)
{
	int r;

	fprintf(stderr, LOG_TAG "waiting for an event\n");
	if (fd_sensor >= 0) {
		int read_len;
		char c;

		c = SENSOR_CMD_START;
		if (write(fd_sensor, &c, 1) < 0) {
			fprintf(stderr, "failed to write sensor command\n");
			return -1;
		}
		if ((read_len = read(fd_sensor, &c, 1)) < 1) {
			fprintf(stderr, "failed to read sensor response\n");
			return -1;
		}
		if (c != SENSOR_CMD_DETECTED) {
			fprintf(stderr,
				"invalid sensor response (0x%02x)\n", c);
			return -1;
		}
		c = SENSOR_CMD_ACTIVE;
		if (write(fd_sensor, &c, 1) < 0) {
			fprintf(stderr, "failed to write sensor command\n");
			return -1;
		}
	} else {
		int c;
		do {
			c = getchar();
			if (c == 'q')	/* quit command */
				return 0;
		} while (c != '\n');
	}

	if (add_callbacks(recog) < 0)
		return -1;

	if (j_adin_init(recog) == FALSE) {
		fprintf(stderr, "failed to initialize audio input\n");
		return -1;
	}

	/* output system information to log */
	if (app.verbose)
		j_recog_info(recog);

	r = j_open_stream(recog, NULL);
	if (r < 0) {
		if (r == -1)		/* error */
			fprintf(stderr, "error in input stream\n");
		else if (r == -2)	/* end of recognition process */
			fprintf(stderr, "failed to begin input stream\n");
		return -1;
	}

	if (j_recognize_stream(recog) < 0)
		return -1;

	if (fd_sensor >= 0) {
		char c;

		c = SENSOR_CMD_INACTIVE;
		if (write(fd_sensor, &c, 1) < 0) {
			fprintf(stderr, "failed to write sensor command\n");
			return -1;
		}
	}

	return 1;
}

static int sensor_sock_open(const char *host_name, const char *port_str)
{
	int fd;
	struct addrinfo ai_hint, *aip;
	int r;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "socket() failed\n");
		return -1;
	}

	memset(&ai_hint, 0, sizeof(struct addrinfo));
	ai_hint.ai_family = AF_INET;
	ai_hint.ai_socktype = SOCK_STREAM;
	ai_hint.ai_flags = 0;
	ai_hint.ai_protocol = 0;

	if ((r = getaddrinfo(host_name, port_str, &ai_hint, &aip)) < 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(r));
		close(fd);
		return -1;
	}

	if (connect(fd, aip->ai_addr, aip->ai_addrlen) < 0) {
		fprintf(stderr, "connect() failed\n");
		close(fd);
		freeaddrinfo(aip);
		return -1;
	}

	freeaddrinfo(aip);

	return fd;
}

static int sensor_sock_close(int fd)
{
	return close(fd);
}

static int setup_stdin(struct termios *stdtio_old)
{
	struct termios stdtio_new;

	tcgetattr(0, stdtio_old);

	memcpy(&stdtio_new, stdtio_old, sizeof(struct termios));
	stdtio_new.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(0, TCSANOW, &stdtio_new);

	return 0;
}

static void restore_stdin(const struct termios *stdtio_old)
{
	tcsetattr(0, TCSANOW, stdtio_old);
}

int parse_arg(int argc, char **argv)
{
	char *cpy_path;
	int r = 0;
	int i;

	/*
	 * default settings
	 */
	app.squash_server = NULL;
	app.has_sensor = 0;
	app.verbose = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--squash")) {
			if (++i == argc) {
				r = 1;
				goto usage;
			}
			app.squash_server = argv[i];
		} else if (!strcmp(argv[i], "--sensor")) {
			app.has_sensor = 1;
		} else if (!strcmp(argv[i], "--verbose")) {
			app.verbose = 1;
		} else if (!strcmp(argv[i], "--help")) {
			goto usage;
		} else {
			r = 1;
			goto usage;
		}
	}

	/* sanity check */
	if (app.has_sensor && !app.squash_server) {
		fprintf(stderr,
			"specified --sensor, but --squash is not given.\n");
		r = 1;
		goto usage;
	}

	return 0;

usage:
	cpy_path = strdup(argv[0]);
	fprintf(stderr,
"usage : %s\n"
"        [--squash <host>]\n"
"        [--sensor]\n"
"        [--verbose]\n"
"        [--help]\n",
		basename(cpy_path));
	free(cpy_path);
	exit(r);
}

int main(int argc, char *argv[])
{
	Jconf *jconf;
	Recog *recog;
	int fd_sensor = -1;
	struct termios stdtio_old;

	if (parse_arg(argc, argv) < 0)
		return 1;

	if (!app.verbose)
		jlog_set_output(NULL);

	jconf = j_config_load_file_new(JCONF_FILE);
	if (jconf == NULL) {
		fprintf(stderr, "error in %s", JCONF_FILE);
		return 1;
	}

	recog = j_create_instance_from_jconf(jconf);
	if (recog == NULL) {
		fprintf(stderr, "failed to create recognizer instance\n");
		return 1;
	}

	if (app.has_sensor) {
		fd_sensor = sensor_sock_open(app.squash_server,
					     SENSOR_PORT_STR);
		if (fd_sensor < 0)
			return 1;
	}

	setup_stdin(&stdtio_old);

	for (;;) {
		int r = main_loop(fd_sensor, recog);
		if (r < 0)
			return 1;
		else if (r == 0)
			break;
	}

	restore_stdin(&stdtio_old);

	j_recog_free(recog);

	if (app.has_sensor)
		sensor_sock_close(fd_sensor);

	return 0;
}
