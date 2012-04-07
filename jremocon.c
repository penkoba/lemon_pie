#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <julius/juliuslib.h>

#define LOG_TAG		"[jremocon] "
#define JCONF_FILE	"remocon.jconf"
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(a[0]))

static struct {
	int verbose;
} g_sysopt;

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
	{ "G+",				"tv_component1 catv_catv catv_input_ch"
					" catv_4 catv_2 catv_9" },
	{ "ファミ劇",			"tv_component1 catv_catv catv_input_ch"
					" catv_5 catv_5 catv_8" },
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
			int k;
			const char *wstr = winfo->woutput[snt->word[j]];

			for (k = 0; k < ARRAY_SIZE(cmd_table); k++) {
				if (!strcmp(wstr, cmd_table[k].word)) {
					char cmd[256];
					sprintf(cmd,
						"lemon_corn -proxy localhost"
						" %s",
						cmd_table[k].lc_cmd);
					system(cmd);
				}
			}
		}

		/* score */
		if (g_sysopt.verbose)
			printf("score%d: %f\n", i + 1, snt->score);
	}

	j_close_stream(recog);
	clear_callbacks(recog);
}

int parse_arg(int argc, char **argv)
{
	char *cpy_path;
	int r = 0;
	int i;

	/*
	 * default settings
	 */
	g_sysopt.verbose = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--verbose")) {
			g_sysopt.verbose = 1;
		} else if (!strcmp(argv[i], "--help")) {
			goto usage;
		} else {
			r = 1;
			goto usage;
		}
	}

	return 0;

usage:
	cpy_path = strdup(argv[0]);
	fprintf(stderr, "usage : %s [--verbose]\n", basename(cpy_path));
	free(cpy_path);
	exit(r);
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

int main(int argc, char *argv[])
{
	Jconf *jconf;
	Recog *recog;
	int r;

	if (parse_arg(argc, argv) < 0)
		return 1;

	if (!g_sysopt.verbose)
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

	if (add_callbacks(recog) < 0)
		return 1;

	if (j_adin_init(recog) == FALSE) {
		fprintf(stderr, "failed to initialize audio input\n");
		return 1;
	}

	/* output system information to log */
	if (g_sysopt.verbose)
		j_recog_info(recog);

	r = j_open_stream(recog, NULL);
	if (r < 0) {
		if (r == -1)		/* error */
			fprintf(stderr, "error in input stream\n");
		else if (r == -2)	/* end of recognition process */
			fprintf(stderr, "failed to begin input stream\n");
		return 1;
	}

	if (j_recognize_stream(recog) < 0)
		return 1;

	j_recog_free(recog);

	return 0;
}
