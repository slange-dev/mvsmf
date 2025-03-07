#include <stddef.h> // Füge diese Zeile hinzu
#include <stdio.h>

#include "authmw.h"
#include "logmw.h"
#include "dsapi.h"
#include "httpd.h"
#include "infoapi.h"
#include "jobsapi.h"
#include "router.h"

int main(int argc, char **argv) 
{
	int irc = 0;

	CLIBPPA *ppa = __ppaget();
	CLIBGRT *grt = __grtget();
	CLIBCRT *crt = __crtget();

	void *crtapp1 = NULL;
	void *crtapp2 = NULL;

	HTTPD *httpd = grt->grtapp1;
	HTTPC *httpc = grt->grtapp2;

	Router router = {.routes = 0, .middlewares = 0};
	Session session = {.router = &router, .httpd = httpd, .httpc = httpc};

	if (!httpd) {
		wtof("This program %s must be called by the HTTPD web server%s", argv[0], "");

		/* TSO callers might not see a WTO message, so we send a STDOUT message too */
		printf("This program %s must be called by the HTTPD web server%s", argv[0], "\n");

		return 12;
	}

	/* save for our exit/external programs */
	if (crt) {
		crtapp1 = crt->crtapp1;
		crtapp2 = crt->crtapp2;
		crt->crtapp1 = httpd;
		crt->crtapp2 = httpc;
	}

	/* TODO (mig): move this into cgxstart.c */
	init_router(&router);
	init_session(&session, &router, httpd, httpc);

	add_middleware(&router, "Authentication", authentication_middleware);

#if 0
	add_middleware(&router, "Logging", logging_middleware);
#endif

	/* add the URL mappings */
	add_route(&router, GET, "/zosmf/info", infoHandler);

	add_route(&router, GET, "/zosmf/restjobs/jobs", jobListHandler);
	add_route(&router, GET, "/zosmf/restjobs/jobs/{job-name}/{jobid}/files", jobFilesHandler);
	add_route(&router, GET, "/zosmf/restjobs/jobs/{job-name}/{jobid}/files/{ddid}/records", jobRecordsHandler);
	add_route(&router, PUT, "/zosmf/restjobs/jobs", jobSubmitHandler);
	add_route(&router, GET, "/zosmf/restjobs/jobs/{job-name}/{jobid}", jobStatusHandler);
	add_route(&router, DELETE, "/zosmf/restjobs/jobs/{job-name}/{jobid}", jobPurgeHandler);

	add_route(&router, GET, "/zosmf/restfiles/ds", datasetListHandler);
	add_route(&router, GET, "/zosmf/restfiles/ds/{dataset-name}", datasetGetHandler);
	add_route(&router, GET, "/zosmf/restfiles/ds/-({volume-serial})/{dataset-name}", datasetGetHandler);
	add_route(&router, PUT, "/zosmf/restfiles/ds/{dataset-name}", datasetPutHandler);
	add_route(&router, PUT, "/zosmf/restfiles/ds/-({volume-serial})/{dataset-name}", datasetPutHandler);
	add_route(&router, GET, "/zosmf/restfiles/ds/{dataset-name}/member", memberListHandler);
	add_route(&router, GET, "/zosmf/restfiles/ds/{dataset-name}({member-name})", memberGetHandler);
	add_route(&router, PUT, "/zosmf/restfiles/ds/{dataset-name}({member-name})", memberPutHandler);

	/* dispatch the request */
	irc = handle_request(&router, &session);

quit:

	/* TODO (mig): the whole security stuff must be reworked */
	if (session.acee) {
		irc = racf_logout(&session.acee);
		racf_set_acee(session.old_acee);
		session.acee = NULL;
		session.old_acee = NULL;
	}

	/* restore crt values */
	if (crt) {
		crt->crtapp1 = crtapp1;
		crt->crtapp2 = crtapp2;
	}

	return 0;
}

