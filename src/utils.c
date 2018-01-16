#include "config.h"
#define _GNU_SOURCE // asprintf
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#ifdef __linux__
#include <sys/socket.h>
#elif _WIN32
#include <winsock2.h>
#endif
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#ifdef __linux__
#include <sys/ioctl.h>
#include <limits.h>
#elif _WIN32
#include <winioctl.h>
#endif

//#include <net/if.h>
#include <sys/time.h>
#include <sys/file.h>
//#include <sys/utsname.h>
#include <sys/types.h>
//#include <netdb.h>
//#include <netinet/tcp.h>
#include <signal.h>
//#include <dlfcn.h>
#include <json-c/json.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdarg.h>
#include "utils.h"
#include "unp.h"
#include "debug.h"
#include "byteconversion.h"
#include "local_simet_config.h"
#include "netsimet.h"
#include "gopt.h"


char *detecta_interface_std_gw () {
	char *ret_func;
    ret_func = chama_pipe("netstat -nr | grep UG | awk '{print $8}'", "falha ao detectar interface std_gw\n");
	if (!ret_func) {
		ERROR_PRINT ("nao conseguiu detectar interface std_gw\n");
		return NULL;
    }
	return ret_func;
}

sized_buffer* gera_md5 (sized_buffer *sbuf) {
	EVP_MD_CTX *mdctx;
	const EVP_MD *md;
	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;
	sized_buffer *checksum;

	OpenSSL_add_all_digests();
	md = EVP_get_digestbyname("MD5");
	if(!md) {
		ERROR_PRINT ("message digest MD5 desconhecido");
		return NULL;
	}

	mdctx = EVP_MD_CTX_create();
	EVP_DigestInit_ex(mdctx, md, NULL);
	EVP_DigestUpdate(mdctx, sbuf->buffer, sbuf->size);
	EVP_DigestFinal_ex(mdctx, md_value, &md_len);
	EVP_MD_CTX_destroy(mdctx);

	checksum = (sized_buffer*) malloc (sizeof (sized_buffer));
	if (!checksum) {
		ERROR_PRINT ("aloc md5");
		return NULL;
	}
	memset (checksum, 0, sizeof (sized_buffer));
	if (armazena_dados_s_buffer (checksum, (char*) md_value, (int32_t) md_len) < (int32_t) md_len) {
		ERROR_PRINT ("aloc sbuf md5");
		libera_sized_buffer (checksum);
		return NULL;
	}
/*	memset (checksum, 0, tam_char_md5 + 1);
	for(i = 0; i < md_len; i++) {
		sprintf(trecho_md5, "%02x", md_value[i]);
		memcpy (checksum + 2 * i, trecho_md5, 2);
	}
*/
	/* Call this once before exit. */
	EVP_cleanup();
	return checksum;
}


uint32_t armazena_dados_s_buffer (sized_buffer *sbuf, void *dados, int32_t tam_dados) {
	// assume posse do mutex
	char *aux;
	uint32_t tam_total, bytes_adicionados;

	if (sbuf->size == INT32_MAX)
		return 0;
	if ((uint64_t) (sbuf->size + tam_dados) > INT32_MAX) {
		// maior que buffer máximo
		tam_total = INT32_MAX;
	}
	else tam_total = sbuf->size + tam_dados;

	if (sbuf->alloc_size < tam_total) {
		aux = realloc (sbuf->buffer, tam_total);
		if (!aux) {
			return 0;
		}
		else {
			sbuf->buffer= aux;
			sbuf->alloc_size = tam_total;
		}
	}

	memcpy (sbuf->buffer + sbuf->size, dados, tam_total - sbuf->size);
	bytes_adicionados = tam_total - sbuf->size;
	sbuf->size = tam_total;
	return bytes_adicionados;
}

void limpa_sized_buffer (sized_buffer *sbuf) {
	if (sbuf->buffer) {
		free (sbuf->buffer);
		sbuf->buffer = NULL;
	}
	sbuf->size = sbuf->alloc_size = 0;
}
void libera_sized_buffer (sized_buffer *sbuf) {
	if (sbuf) {
		limpa_sized_buffer (sbuf);
		free (sbuf);
		sbuf = NULL;
	}
}

uint32_t remove_bytes_buffer (sized_buffer *sbuf, uint32_t tam) {
	int i, j;
	if (tam > sbuf->size)
		return 0;
	else {
		for (i = 0, j = tam; j < sbuf->size; i++, j++)
			sbuf->buffer[i] = sbuf->buffer [j];
		sbuf->size -= tam;
	}
	if (sbuf->size == 0)
		limpa_sized_buffer (sbuf);
	return tam;
}

/*
void *link_biblioteca_dinamica() {
	void *handle;
	char *error;

	INFO_PRINT ("Conectando biblioteca dinamica: ");
	handle = dlopen ("libjson-c.so.2", RTLD_NOW | RTLD_NODELETE); //libjson-c 0.11
	if (!handle) {
		handle = dlopen ("libjson.so.0.0.1", RTLD_NOW | RTLD_NODELETE);//libjson-c 0.9
		if (!handle) {
			fputs (dlerror(), stderr);
			exit(1);
		}
		else INFO_PRINT ("libjson.so.0.0.1\n");
	}
	else INFO_PRINT ("libjson-c.so.2.0.1\n");



	f_json_tokener_parse = dlsym(handle, "json_tokener_parse");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}
	f_json_tokener_parse_verbose = dlsym(handle, "json_tokener_parse_verbose");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}
	f_json_object_to_json_string = dlsym(handle, "json_object_to_json_string");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	f_json_object_object_get = dlsym(handle, "json_object_object_get");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	f_json_object_get_int = dlsym(handle, "json_object_get_int");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	f_json_object_put = dlsym(handle, "json_object_put");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	f_json_object_get_array = dlsym(handle, "json_object_get_array");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	f_json_object_array_length = dlsym(handle, "json_object_array_length");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	f_json_object_get_type = dlsym(handle, "json_object_get_type");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	f_json_object_get_boolean = dlsym(handle, "json_object_get_boolean");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	f_json_object_get_double = dlsym(handle, "json_object_get_double");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	f_json_object_get_string = dlsym(handle, "json_object_get_string");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	f_json_object_array_get_idx = dlsym(handle, "json_object_array_get_idx");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	f_json_object_get_object = dlsym(handle, "json_object_get_object");
	if (( error = dlerror() ) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	INFO_PRINT ("Link dinamico executado com sucesso!\n");


	return handle;
}
*/

int hexdump (unsigned char *c, int size) {
	long int addr;
	unsigned char buf[20];
	long int cnt;
	long int cnt2;
	FILE *fileout;
	int m, n, piece;


	fileout = stderr;

	fprintf ( fileout, "Imprime %d bytes\n", size);
	fprintf ( fileout, "   Endereco                    Valores em Hexa                      Visiveis\n" );
	fprintf ( fileout, "--------------  -----------------------------------------------  ----------------\n" );
	fprintf ( fileout, "\n" );
	addr = 0;
	while (addr < size) {
		cnt = 16;
		memset (buf, 0, 20);
		piece = MIN (size - addr, 16);
		memcpy (buf, c + addr, piece);
		addr += piece;
		if (addr > size) {
			cnt = piece;
		}

		fprintf ( fileout, "%4d %08lx  ", (int) addr, addr );
		cnt2 = 0;

		for (m = 0; m < piece; m++) {
			fprintf ( fileout, "%02x ", buf [m]);
		}
		for ( m = piece; m < 16; m++ ) {
			fprintf ( fileout, "   " );
		}
		fprintf ( fileout, " " );
		cnt2 = 0;
		for ( n = 0; n < piece; n++ ) {
			cnt2 = cnt2 + 1;
			if ( cnt2 <= cnt) {
				if ( ( buf[n] < 32 ) || ( buf[n] > 126 ) ) {
					fprintf ( fileout, "%c", '.' );
				}
				else {
					fprintf ( fileout, "%c", buf[n] );
				}
			}
		}
		fprintf( fileout, "\n" );
	}
	fprintf ( fileout, "\n" );
	fflush (fileout);
	return SUCCESS;
}


char *chama_pipe (char *comando, char *erro) {
	FILE *fp;
	char path [100];
	char *result = NULL;
	char *temp = NULL;
	int tam = 0, aux;

	fp = popen(comando, "r");
	if (fp == NULL) {
		INFO_PRINT("%s", erro);
		saida (1);
	}

	memset (path, 0, sizeof (path));
	while (fgets(path, sizeof(path) - 1, fp) != NULL) {
		aux = strlen (path);
		temp = (char *) realloc (result, tam + aux + 1);
		if (!temp) {
			INFO_PRINT ("nao conseguiu alocar memoria para comando [%s]", comando);
			saida (1);
		}
		result = temp;
		memset (&result[tam], 0, aux);
		tam += aux;
		strcat (result, path);
	}
	pclose(fp);
	if (result) {
		if (result[strlen(result)-1] == '\n')
			result[strlen(result)-1] = '\0';
		INFO_PRINT ("saida pipe %s:\n[%s]\n", comando, result);
	}
	return result;
}



char *remove_barra_r_barra_n (char * str) {
	char *aux;

	if (!str)
		return NULL;
	aux = strrchr(str, '\r');
	if (aux)
		*aux = '\0';
	return str;
}



void saida (int rc) {
	int i, num_files;

	num_files = sysconf(_SC_OPEN_MAX); /* how many file descriptors? */

	INFO_PRINT ("Liberando lockfile\n");
	if (num_files < 0) /* sysconf has returned an indeterminate value */
		num_files = OPEN_MAX_GUESS; /* from Stevens '93 */

	for (i = num_files - 1; i >= 0; i--) { /* close all open files */
		close(i);
	}
	if (lockFilename) {
		unlink(lockFilename);
		free (lockFilename);
	}
	fflush (stdout);
	exit (rc);
}

int captura_lock (int tentativa, char* lockfile) {

	int	lockresult, kill_result, local_lockfd;
	char pidbuff[17], *lockfilepidstr, pidstr[7];
	FILE *plockfile = NULL;
	uint64_t pid;
	struct flock xlock;

	if (tentativa == 3) {
		INFO_PRINT ("Tentou obter o lockfile por 3 vezes. Desistindo\n");
		return -1;
	}


	local_lockfd = open (lockfile, O_RDWR | O_CREAT | O_EXCL, 0600);

// caso queira usar o flock
/*    local_lockfd = open(lockfile, O_RDWR|O_NOCTTY|O_CREAT, 0666);
		// Linux doesn't like O_CREAT on a directory, even though it should be a no-op
	if (local_lockfd < 0 && errno == EISDIR)
		local_lockfd = open(lockfile, O_RDONLY|O_NOCTTY);
*/

	if (local_lockfd == -1) {
		// tenta abrir o lock file

		plockfile = fopen (lockfile, "r");

		if (!plockfile) {
		// Nao conseguiu abrir o lock file
			INFO_PRINT("Nao conseguiu abrir o lock file");
			return -1;
		}

		// le o PID no arquivo
		lockfilepidstr = fgets (pidbuff, 16, plockfile);

		if (lockfilepidstr!=0) {
			if (pidbuff [strlen (pidbuff) - 1] == '\n') // remove /n no final
				pidbuff [strlen (pidbuff) - 1] = '\0';

			pid = strtoul (pidbuff, (char**) 0, 10);

			// Descobre se o programa ainda esta executando sem mandar um sinal

			kill_result = kill (pid, 0);

			if (kill_result == 0) {
				INFO_PRINT("O lock file %s foi encontrado em execucao e pertence ao processo de PID %"PRUI64".\n", lockfile, pid);
				fclose(plockfile);
				return -1;
			}
			else {
				if(errno == ESRCH) {
				// nao existe o processo
					INFO_PRINT("O lock file %s foi encontrado e pertence ao processo de PID %"PRUI64", que nao executa no momento.\nTentarei apagar o arquivo e obter novamente o lock", lockfile, pid);
					fclose (plockfile);
					if (unlink (lockfile) == -1) {
						INFO_PRINT ("Nao foi possivel deletar o lock file %s. Contate um administrador\n", lockfile);
					}
					sleep (1);
					return captura_lock (tentativa + 1, lockfile);
				}
				else {
					INFO_PRINT("Nao foi possivel obter o lock file para o %s\n", lockfile);
				}
			}
		}
		else {
			INFO_PRINT("Nao foi possivel ler o lock file.\nTentarei apagar o arquivo e obter novamente o lock");
			if (unlink (lockfile) == -1) {
				INFO_PRINT ("Nao foi possivel deletar o lock file %s. Contate um administrador\n", lockfile);
			}
			sleep (1);
			return captura_lock (tentativa + 1, lockfile);
		}

		fclose (plockfile);
		return -1;
	}

	// acesso ao lock file.

	xlock.l_type = F_WRLCK; // escrita exclusiva
	xlock.l_whence = SEEK_SET;
	xlock.l_len = xlock.l_start = 0; // usar o arquivo todo
	xlock.l_pid = 0;
	lockresult = fcntl(local_lockfd,F_SETLK,&xlock);

// caso queira usar flock
//    lockresult = flock(local_lockfd, LOCK_EX | LOCK_NB);
	if (lockresult < 0) {
		close(local_lockfd);
		INFO_PRINT("Nao conseguiu obter o lock file\n");
		return -1;
	}


	if (ftruncate (local_lockfd, 0) < 0) {
		INFO_PRINT("Nao conseguiu limpar o lock file\n");
		return -1;
	}
	// escreve o PID atual no arquivo
	sprintf (pidstr, "%d\n", (int) getpid ());

	INFO_PRINT ("lock file obtido para o PID: %s", pidstr);
	if (write(local_lockfd, pidstr, strlen (pidstr)) != strlen (pidstr)) {
		INFO_PRINT("Nao conseguiu escrever no lock file\n");
		return -1;
	}


	if (plockfile)
		fclose (plockfile);
	return local_lockfd;
}


void *watchdog (void * unused __attribute__((unused)) ) {
	sleep (TEMPO_WATCHDOG);
	INFO_PRINT("watchdog: aborting program\n");
	saida(1);

	/* not reached */
	return NULL;
}


char * get_json_string_from_str (char *string, char *parametro) {
	char *result = NULL;
//	void* handle;
	json_object *jobj, *aux;
	enum json_tokener_error error = json_tokener_success;

//		handle = link_biblioteca_dinamica();
		jobj = json_tokener_parse_verbose(string, &error);
		if (!json_object_object_get_ex(jobj, parametro, &aux)) {
			json_object_put(jobj);
			return NULL;
		}
		result = strdup (json_object_get_string(aux));
		json_object_put(jobj);
//		dlclose(handle);
	if (!result) {
		ERROR_PRINT ("Nao foi possivel obter %s em [%s]\n", parametro, string);
		return NULL;
	}

	return result;
}

char * get_hash_measure (char *host, char *port, int family, char *context_root, char *mac_address, SSL_CTX *ssl_ctx) {
	char *formata_parametro, *result = NULL, *string = NULL, *temp_device;
	int i = 0;

	if (asprintf (&formata_parametro, "/%s/GetHashs?hashDevice=%s&deviceType=%s", context_root, mac_address, DEVICE_TYPE) == -1) {
		ERROR_PRINT ("sem memoria\n");
		saida (1);
	}

	while ((!result) && (i < MAX_HASH_MEASURE_ATTEMPTS)) {
		i++;
		string = chama_web_service_ssl_arg (ssl_ctx, host, port, family, NULL, 0, formata_parametro);

		if (!strnlen(string, 1)) {
			free (string);
			sleep (5);
			string = NULL;
			continue;
		}
		temp_device = get_json_string_from_str (string, "hashDevice");
		if ((temp_device) && (strncmp (mac_address, temp_device, strlen (mac_address)))) {
			ERROR_PRINT ("HASHDEVICE ERRADO");
			free (temp_device);
			free (string);
			sleep (5);
			string = NULL;
			continue;
		}
		free (temp_device);

		result = get_json_string_from_str (string, "hashMeasure");
		if (!result) {
			free (string);
			sleep (5);
			string = NULL;
			continue;
		}

	}
	free (formata_parametro);
	if (!result) {
		ERROR_PRINT ("Nao foi possivel obter HASH_MEASURE\n");
		saida (1);
	}
	if (string)
		free (string);
	return result;

}


int ishex(char *x, int tam) {
	int i;

	for (i = 0; i < tam; i++) {
		if (!(
			(*(x + i) >= '0' && *(x + i) <= '9') ||
			(*(x + i) >= 'A' && *(x + i) <= 'F') ||
			(*(x + i) >= 'a' && *(x + i) <= 'f')))
			return 0;
	}
	return 1;
}

int decode_url (char *url)
{
	char *output, *ini, *fim = url + strlen(url);
	int c = '\0';

	for (ini = output = url; url <= fim; url++, output++) {
		switch (*url) {
			case '+':
				*output = ' ';
				break;
			case '%':
				if ((url + 2 <= fim) && (!ishex((++url), 2) || !sscanf(url++, "%2x", &c)))
					return -1;
				*output = (char) c;
				break;
			default :
				*output = *url;
				break;
		}
	}

	return output - ini;
}


char to_hex(char code) {
	static char hex[] = "0123456789abcdef";
	return hex[code & 15];
}

char *converte_url(char *str) {
// troca caracteres '+' por '%2B', pois '+' é espaço em url
	char *pstr = str, *buf = malloc(strlen(str) * 3 + 1), *pbuf = buf;
	if (!buf)
		return NULL;
	while (*pstr) {
		if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~' || *pstr == ':' || *pstr == '>' || *pstr == '(' || *pstr == ')' || *pstr == '[' || *pstr == ']' || *pstr == '%')
			*pbuf++ = *pstr;
		else if (*pstr == ' ')
			*pbuf++ = '+';
		else
			*pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
		pstr++;
	}
	*pbuf = '\0';
	return buf;
}
char *converte_url2(char *str) {
// troca caracteres '+' por '%2B', pois '+' é espaço em url
	char *pstr = str, *buf = malloc(strlen(str) * 4 + 1), *pbuf = buf;
	if (!buf)
		return NULL;
	while ((*pstr) && (*pstr != '?')) {
		*pbuf++ = *pstr++;
	}
	if (*pstr) {
		*pbuf++ = *pstr++;
		while (*pstr) {
			if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~' || *pstr == '=' || *pstr == '&' || *pstr == ':' || *pstr == '>' || *pstr == '(' || *pstr == ')' || *pstr == '[' || *pstr == ']' || *pstr == '%' || *pstr == '+')
				*pbuf++ = *pstr;
			else if (*pstr == ' ')
				*pbuf++ = '+';
			else
				*pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
			pstr++;
		}
	}
	*pbuf = '\0';
	return buf;
}

void parse_verbose (void *options) {
	int verbosity, dev_null;

	verbosity= gopt( options, 'v' );

	if( verbosity == 5 )
    INFO_PRINT ("being really verbose\n" );

	else {
		// desliga stderr
		dev_null = open("/dev/null", O_WRONLY);
		if(dev_null == -1){
			ERROR_PRINT ("open(/dev/null)\n");
			saida (1);
		}

		if(dup2(dev_null, STDERR_FILENO) == -1) {
			ERROR_PRINT ("dup2\n");
			exit(EXIT_FAILURE);
		}
	}
}

void parse_args_basico (void * options, char doc[], int* family) {

	if (gopt(options, 'h')) {
		fprintf(stdout, "%s", doc);
		exit (EXIT_SUCCESS);
	}
	if (gopt(options, '4')) {
		*family = 4;
	}
	if (gopt(options, '6')) {
		*family = 6;
	}
}

void *args_basico (int *argc, const char *argv[]) {
	void *options =
			gopt_sort(argc, argv,
					gopt_start(
						DEFAULT_OPTS
					));

	parse_verbose (options);
	return options;
}


char *meu_sprintf (const char *fmt, ...) {
	int n;
	int size = 100;     /* Guess we need no more than 100 bytes. */
	char *p, *np;
	va_list ap;

	if ((p = malloc(size)) == NULL)
		return NULL;

	while (1) {

		/* Try to print in the allocated space. */

		va_start(ap, fmt);
		n = vsnprintf(p, size, fmt, ap);
		va_end(ap);

		/* If that worked, return the string. */

		if (n > -1 && n < size)
			return p;

		/* Else try again with more space. */

		if (n > -1)    /* glibc 2.1 */
			size = n+1; /* precisely what is needed */
		else           /* glibc 2.0 */
			size *= 2;  /* twice the old size */

		if ((np = realloc (p, size)) == NULL) {
			free(p);
			return NULL;
		}
		else {
			p = np;
		}
	}
}

void separa_protocolo_host_chamada (URL_PARTS *urlp, char* url) {
	char *aux, *aux2, *aux3;
	int tam;

	if (decode_url(url) < 0) {
		ERROR_PRINT ("invalid url");
		saida (1);
	}
	aux = url;

	if (strncmp (aux, HTTPS, strlen (HTTPS)) == 0) {
		urlp->https = 1;
		aux3 = aux + strlen (HTTPS);
	}
	else if (strncmp (aux, HTTP, strlen (HTTP)) == 0) {
		urlp->https = 0;
		aux3 = aux + strlen (HTTP);
	}
	else { // por padrão usar HTTPS
		urlp->https = 1;
		aux3 = aux;
	}

	if ((aux2 = strchr (aux3, '/'))) {
		tam = aux2 - aux3;
		urlp->host = strndup (aux3, tam);
		urlp->chamada = strdup (aux2);
	}
	else {
		urlp->host = strdup (aux3);
		urlp->chamada = strdup ("/");
	}

}

char* le_arq (char *nome_arq) {
	FILE *fp;
	char *result = NULL;
	int string_size, read_size;

	if((fp = fopen(nome_arq, "r"))==NULL) {
		INFO_PRINT ("não pode abrir arquivo %s.\n", nome_arq);
		saida (1);
	}

	fseek (fp, 0, SEEK_END);
	string_size = ftell (fp);
	rewind(fp);
	result = (char *) malloc (sizeof (char) * (string_size + 1));
	if (!result) {
		INFO_PRINT ("erro ao alocar mem para arquivo de entrada\n");
		fclose (fp);
		saida (1);
	}
	read_size = fread (result, sizeof(char), string_size, fp);
	if (read_size > string_size) {
		INFO_PRINT ("erro ao ler arquivo de entrada\n");
		fclose (fp);
		saida (1);
	}
	fclose (fp);
	result [read_size] = '\0';
	return result;
}


uint64_t get_rand_i () {
	int fd;
	int64_t rand;


	fd = open("/dev/urandom", O_RDONLY);
	if (fd != -1)
	{
		if (!read(fd, (void *)&rand, sizeof(uint64_t))) {
			ERROR_PRINT ("nao foi possivel ler /dev/urandom\n");
			saida (1);
		}
		close(fd);
	}
	return rand;
}

char* obtem_modelo (int *pc) {
	char *modelo_digitos, *ret_func;

    ret_func = chama_pipe("/usr/bin/get_model.sh", "falha ao executar o script para detectar o hardware\n");
    if (ret_func) {
        modelo_digitos = ret_func;
    }
    else {
    // não bateu com nenhum modelo
        ret_func = chama_pipe("/bin/cat /proc/cpuinfo |grep vendor_id", "falha ao executar o programa cat para detectar o hardware\n");
        if (ret_func) {
            if (strstr (ret_func, "GenuineIntel")) {
				if (pc)
					*pc = 1;
                INFO_PRINT ("Executando a partir de um PC. Assumindo um valor válido para modelo do simetbox\n");
                modelo_digitos = meu_sprintf ("07400004");
				if (!modelo_digitos) {
					ERROR_PRINT ("%s\n", NAO_ALOCA_MEM);
					saida (1);
				}
            }
            free (ret_func);
        }
        else {
            INFO_PRINT ("Nao foi encontrado modelo do simetbox. Abortando");
            saida (1);
        }
    }

    INFO_PRINT ("modelo: [%s]\n", modelo_digitos);
	return modelo_digitos;
}

char *obtem_versao () {
	char *build = NULL, *ret_func, *pbuild, *pfim_build;

    ret_func = chama_pipe ("/bin/uname -v", "falha ao executar o programa uname para detectar a versao do sistema operacional\n");
    if (ret_func) {
        pbuild = strchr (ret_func, '#');
        if (pbuild) {
            pbuild++;
            pfim_build = pbuild;
            while (isdigit (*pfim_build)) {
                pfim_build++;
            }
            if (pfim_build)
                *pfim_build = '\0';
            build = strdup (pbuild);
        }
        free (ret_func);
    }
    else {
		INFO_PRINT ("Nao foi encontrada versao do simetbox. Abortando");
		saida(1);
	}
	return build;
}

int escreve_arquivo_nome (char *nome_arq, char *conteudo, int size, char *modo) {
	FILE *fp;
	int rc;

	if((fp = fopen(nome_arq, modo)) == NULL) {
		INFO_PRINT("nao pode abrir arquivo %s para escrita.\n", nome_arq);
		return (1);
	}
	rc = fwrite(conteudo, 1, size, fp);
	if (rc != size)
		INFO_PRINT ("Escrita do arquivo com problemas");
	fclose (fp);
	return size - rc;
}

int escreve_arquivo_fd (int fd, char *conteudo, int size) {
	int rc;

	rc = write(fd, conteudo, size);
	if (rc != size)
		INFO_PRINT ("Escrita do arquivo com problemas");
	close (fd);
	return size - rc;
}

int meu_pthread_create (pthread_t *thread, size_t tam_stack_thread, void *(*start_routine) (void *), void *arg) {
	pthread_attr_t attrs;
	pthread_attr_init (&attrs);
	if (tam_stack_thread >= PTHREAD_STACK_MIN)
		pthread_attr_setstacksize (&attrs, tam_stack_thread);
	else
		ERROR_PRINT ("tamanho de pilha deve ser maior que PTHREAD_STACK_MIN: %d", PTHREAD_STACK_MIN);
	pthread_attr_getstacksize (&attrs, &tam_stack_thread);

	return pthread_create (thread, &attrs, start_routine, arg);
}


char *velocidade (int64_t size, int64_t tempo_total) {
	char *result, unidade = '\0';
	double valor = 8 * 1e6 * size / tempo_total;

	if ((valor >= 1e3) && (valor < 1e6)) {
		unidade = 'k';
		valor /= 1e3;
	}
	else if ((valor >= 1e6) && (valor < 1e9)) {
		unidade = 'm';
		valor /= 1e6;
	}
	else if ((valor >= 1e9) && (valor < 1e12)) {
		unidade = 'g';
		valor /= 1e9;
	}

	if (asprintf (&result, "%.2lf %c", valor, unidade) == -1) {
		ERROR_PRINT ("sem memoria\n");
		saida (1);;
	}
	return result;
}

uint64_t uint64_t_pipe(char *temp, char *erro) {
	uint64_t result;
	char *ret_func;
	ret_func = chama_pipe(temp, erro);
	if (ret_func) {
		result = atoll (ret_func);
		free (ret_func);
	}
	else {
		ERROR_PRINT ("pipe\n");
		saida (1);
	}
	return result;
}


void *thread_trata_sinais_vazao (void *arg) {
	int err, signo;
	signal_thread_vazao_arg *signal_arg;
	signal_arg = (signal_thread_vazao_arg*) arg;
	pthread_mutex_lock (&signal_arg->mutex);



	INFO_PRINT ("Thread Sinais Vazao\n");

	while (1) {
		err = sigwait(signal_arg->mask, &signo);
		if (err != 0) {
			INFO_PRINT ("falha sigwait\n");
			saida (1);
		}
		switch (signo) {
			case SIGALRM:
				__sync_fetch_and_add(&signal_arg->indice_bloco, 1);
				if (signal_arg->indice_bloco == signal_arg->max_blocos) {
					INFO_PRINT ("desliga timer...\n");
 					signal_arg->timer->it_value.tv_sec = signal_arg->timer->it_interval.tv_sec = 0;
 					signal_arg->timer->it_value.tv_usec = signal_arg->timer->it_interval.tv_usec = 0;
 					setitimer (ITIMER_REAL, signal_arg->timer, NULL);
					INFO_PRINT ("desligado\n");
					pthread_cond_wait (&signal_arg->cond, &signal_arg->mutex);
					pthread_mutex_unlock (&signal_arg->mutex);
					pthread_exit(NULL);
				}
				//			INFO_PRINT ("[alarm], connected %d, time_now %ld\n", connected, time_now);
				break;

			default:
				INFO_PRINT("sinal inesperado %d\n", signo);
		}
	}
	INFO_PRINT ("unlock mutex Thread Sinais Vazao\n");
	pthread_mutex_unlock (&signal_arg->mutex);
	INFO_PRINT ("Fim Thread Sinais Vazao\n");
	pthread_exit (NULL);
}
uint32_t particiona (int64_t *list, uint32_t left, uint32_t right, uint32_t pivotIndex)
{
    int64_t pivotValue, aux;
    uint32_t storeIndex, i;


    pivotValue = list[pivotIndex];
    aux = list[pivotIndex];
    list[pivotIndex] = list[right];  // Move pivot to end
    list[right] = aux;  // Move pivot to end
    storeIndex = left;

    for (i = left; i < right; i++)
        if (list[i] <= pivotValue)
        {
            aux = list[storeIndex];
            list[storeIndex] = list[i];
            list[i] = aux;
            storeIndex++;
        }
    aux = list[right];
    list[right] = list[storeIndex];  // Move pivot to its final place
    list[storeIndex] = aux;
    return storeIndex;
}


int64_t mediana (int64_t *list, int32_t right)
{
    uint32_t left, k;
    uint32_t pivotNewIndex;

    left = 0;
    k = right / 2;
    while (left < right)
    {
        pivotNewIndex = particiona(list, left, right, k);

        if (pivotNewIndex == k)
            return list[pivotNewIndex];
        else if (k < pivotNewIndex)
            right = pivotNewIndex - 1;
        else
            left = pivotNewIndex + 1;
    }
    return list[left]; // left == right
}
void remove_comentarios_e_barra_n (char *file) {
	char *source, *destination;
	int entre_aspas = 0;

	for (source = file, destination = file; *source != '\0'; source++) {
		if (*source == '"')
			entre_aspas = 1 - entre_aspas;

		else if ((!entre_aspas) && (isblank(*source)))
			continue;
		else if ((*source == '\n') || (*source == '\r'))
			continue;
		*destination++ = *source;
	}
	*destination = '\0';
}
