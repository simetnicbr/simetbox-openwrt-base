#include <stdio.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "simet_content_provider.h"
#include "debug.h"
#include "utils.h"
#include "local_simet_config.h"
#include "netsimet.h"
#include "gopt.h"

int main_content (const char *nome, char *mac_address, int argc, const char* argv[], CONFIG_SIMET* config, SSL_CTX * ssl_ctx, void *options) {
	char *hash_measure;
	const char versao [] = VERSAO_CONTENT;
	char *ret_ws, *p, *ip_fb, *local_google = NULL;
	char redirector_google[] = "https://redirector.c.youtube.com/report_mapping";
	char doc[] =
		"uso:\n\tsimet_content_povider\n\n\tTesta a localização geografica de sites que usam CDN\n";
	int len, proto, family = 0;
	proto_disponiveis disp;
	URL_PARTS urlparts, *urlpartsp;
	urlpartsp = &urlparts;
	int64_t size = 0;


	if (options)
		parse_args_basico (options, doc, &family);

	INFO_PRINT("%s versao %s", nome, versao);
	protocolos_disponiveis (&disp, family, 1, config, ssl_ctx);


	hash_measure = get_hash_measure (config->host, config->port, family, config->context_root, mac_address, ssl_ctx);
	for (proto = PROTO_IPV4; proto <= PROTO_IPV6; proto++) {
		if (!disp.proto [proto])
			continue;

		// facebook
		ip_fb = get_ip_by_host ("www.facebook.com", (proto == PROTO_IPV4)? 4 : 6);
		if (ip_fb) {
//			ret_ws = chama_web_service (ssl_ctx, config->host, "80", 0, NULL, 0, "/%s/ContentService?type=FACEBOOKSERVERLOCATION&hashMeasure=%s&description=%s", config->context_optional, hash_measure, ip_fb);
			ret_ws = chama_web_service_ssl_arg (ssl_ctx, config->host, config->port, 0, NULL, 0, "/%s/ContentService?type=FACEBOOKSERVERLOCATION&hashMeasure=%s&description=%s", config->context_optional, hash_measure, (strnlen(ip_fb,1) > 0)?ip_fb:"-1");
			if (ret_ws)
				free (ret_ws);
			free (ip_fb);
		}

		// google
		separa_protocolo_host_chamada (urlpartsp, redirector_google);
		if (urlpartsp->https) {
			ret_ws = chama_web_service_ssl_arg (ssl_ctx, urlpartsp->host, PORT_HTTPS , (proto == PROTO_IPV4)? 4 : 6, &size, 0, urlpartsp->chamada);
		}
		else {
			ret_ws = chama_web_service (ssl_ctx, urlpartsp->host, PORT_HTTP, (proto == PROTO_IPV4)? 4 : 6, &size, 0, urlpartsp->chamada);
		}
		if (ret_ws) {
			len = strnlen (ret_ws, size);
			if ((len >= 0) && (p = strrchr (ret_ws, '\n')))
				*p = '\0';
			local_google = converte_url (ret_ws);
//			INFO_PRINT ("local_google: [%s]\n", local_google);
			free (ret_ws);
			if (local_google) {
//				ret_ws = chama_web_service (ssl_ctx, config->host, "80", 0, NULL, 0, "/%s/ContentService?type=GOOGLESERVERLOCATION&hashMeasure=%s&description=%s", config->context_optional, hash_measure, local_google);
				ret_ws = chama_web_service_ssl_arg (ssl_ctx, config->host, config->port, 0, NULL, 0, "/%s/ContentService?type=GOOGLESERVERLOCATION&hashMeasure=%s&description=%s", config->context_optional, hash_measure, (strnlen(local_google,1) > 0)?local_google:"-1");
				if (ret_ws)
					free (ret_ws);
				free (local_google);
				local_google = NULL;
			}
		}
	}



	free (hash_measure);


	return 0;
}
