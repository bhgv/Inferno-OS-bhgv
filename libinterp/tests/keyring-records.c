/* Record tests use native key layouts/codecs; these adapters isolate VM string/heap access. */
static SigAlg*
checkSigAlg(Keyring_SigAlg *sa)
{
	return (SigAlg*)sa;
}

/* Fixture strings are immutable ASCII views; conversion itself is outside this test. */
char*
string2c(String *s)
{
	return (char*)s;
}

RECORD_IMPLEMENTATION

/* Compare complete records against independently assembled headers and field vectors. */
static int
recordserialize(int kind, SigAlg *sa, char *owner, long expiration, char *buf, int len)
{
	SK sk;
	PK pk;
	Certificate cert;

	memset(&sk, 0, sizeof sk);
	memset(&pk, 0, sizeof pk);
	memset(&cert, 0, sizeof cert);
	sk.x.sa = pk.x.sa = cert.x.sa = (Keyring_SigAlg*)sa;
	sk.x.owner = pk.x.owner = cert.x.signer = (String*)owner;
	cert.x.ha = (String*)"sha1";
	cert.x.exp = expiration;
	if(sa->vec->sk2str == rsa_sk2str){
		sk.key = &rsakey;
		pk.key = &rsakey.pub;
		cert.signa = &values[0];
	}else if(sa->vec->sk2str == dsa_sk2str){
		sk.key = &dsakey;
		pk.key = &dsakey.pub;
		cert.signa = &dsasignature;
	}else{
		sk.key = &egkey;
		pk.key = &egkey.pub;
		cert.signa = &egsignature;
	}
	switch(kind){
	case 0: return sktostr(&sk, buf, len);
	case 1: return pktostr(&pk, buf, len);
	default: return certtostr(&cert, buf, len);
	}
}

/* Check every capacity for all algorithms, including host-long expiration boundaries. */
static void
recordbounds(void)
{
	SigAlgVec vecs[3];
	SigAlg sa;
	long expirations[] = {0, 1, 2147483647L, LONG_MAX, LONG_MIN};
	char expected[512], output[520], decimal[32];
	int alg, kind, exp, i, count, need, cap, n;

	memset(vecs, 0, sizeof vecs);
	vecs[0].sk2str = rsa_sk2str; vecs[0].pk2str = rsa_pk2str; vecs[0].sig2str = rsa_sig2str;
	vecs[1].sk2str = dsa_sk2str; vecs[1].pk2str = dsa_pk2str; vecs[1].sig2str = dsa_sig2str;
	vecs[2].sk2str = eg_sk2str; vecs[2].pk2str = eg_pk2str; vecs[2].sig2str = eg_sig2str;
	for(alg = 0; alg < 3; alg++){
		sa.vec = &vecs[alg];
		sa.x.name = (String*)(alg == 0 ? "rsa" : alg == 1 ? "dsa" : "elgamal");
		for(kind = 0; kind < 3; kind++)
		for(exp = 0; exp < nelem(expirations); exp++){
			snprintf(decimal, sizeof decimal, "%ld", expirations[exp]);
			snprintf(expected, sizeof expected, kind == 2 ? "%s\nsha1\nowner\n%s\n" : "%s\nowner\n",
				(char*)sa.x.name, decimal);
			/* Use the same independent base64 vectors as the lower-level field oracle. */
			count = fieldcounts[alg*3+kind];
			for(i = 0; i < count; i++){
				strcat(expected, texts[i]);
				strcat(expected, "\n");
			}
			need = strlen(expected);
			for(cap = 0; cap <= need+2; cap++){
				memset(output, 0xa5, sizeof output);
				n = recordserialize(kind, &sa, "owner", expirations[exp], output+1, cap);
				if(cap <= need){
					assert(n == -1);
					if(cap > 0)
						assert(output[1] == 0);
				}else
					assert(n == need && strcmp(output+1, expected) == 0);
				assert((uchar)output[0] == 0xa5 && (uchar)output[cap+1] == 0xa5);
				assert(live == 0);
				checks++;
			}
		}
	}
}

/* Invalid header boundaries and exhausted buffers must not invoke the payload serializer. */
static int recordcalls;
static int
recordpayload(void *value, char *buf, int len)
{
	USED(value);
	recordcalls++;
	if(len < 3)
		return -1;
	strcpy(buf, "x\n");
	return 2;
}

/* Exercise metadata rejection separately from cryptographic payload allocation failures. */
static void
recordfailures(void)
{
	char output[32], *headers[2];
	int i;

	headers[0] = "rsa";
	headers[1] = "owner";
	recordcalls = 0;
	assert(keyringrecord(nil, 32, headers, 2, nil, recordpayload) == -1);
	assert(keyringrecord(output, -1, headers, 2, nil, recordpayload) == -1);
	assert(keyringrecord(output, sizeof output, nil, 2, nil, recordpayload) == -1);
	assert(keyringrecord(output, sizeof output, headers, -1, nil, recordpayload) == -1);
	assert(keyringrecord(output, sizeof output, headers, 2, nil, nil) == -1);
	checks += 5;
	for(i = 0; i < 2; i++){
		headers[i] = "injected\nfield";
		memset(output, 0xa5, sizeof output);
		assert(keyringrecord(output, sizeof output, headers, 2, nil, recordpayload) == -1);
		assert(output[0] == 0 && (uchar)output[1] == 0xa5);
		headers[i] = nil;
		assert(keyringrecord(output, sizeof output, headers, 2, nil, recordpayload) == -1);
		headers[i] = i == 0 ? "rsa" : "owner";
		checks += 2;
	}
	assert(recordcalls == 0);
	assert(keyringrecord(output, 8, headers, 2, nil, recordpayload) == -1 && recordcalls == 0);
	assert(keyringrecord(output, 11, headers, 2, nil, recordpayload) == -1 && output[0] == 0);
	assert(keyringrecord(output, sizeof output, headers, 2, nil, recordpayload) == 12);
	assert(strcmp(output, "rsa\nowner\nx\n") == 0);
	checks += 4;
}

/* A failure in any native integer export must invalidate the whole prefixed record. */
static void
recordfaults(void)
{
	SigAlgVec vecs[3];
	SigAlg sa;
	char output[256];
	int alg, kind, field, count;

	memset(vecs, 0, sizeof vecs);
	vecs[0].sk2str = rsa_sk2str; vecs[0].pk2str = rsa_pk2str; vecs[0].sig2str = rsa_sig2str;
	vecs[1].sk2str = dsa_sk2str; vecs[1].pk2str = dsa_pk2str; vecs[1].sig2str = dsa_sig2str;
	vecs[2].sk2str = eg_sk2str; vecs[2].pk2str = eg_pk2str; vecs[2].sig2str = eg_sig2str;
	for(alg = 0; alg < 3; alg++){
		sa.vec = &vecs[alg];
		sa.x.name = (String*)"algorithm";
		for(kind = 0; kind < 3; kind++){
			count = fieldcounts[alg*3+kind];
			for(field = 1; field <= count; field++){
				attempts = 0;
				failat = field;
				memset(output, 0xa5, sizeof output);
				assert(recordserialize(kind, &sa, "owner", 1, output, sizeof output) == -1);
				assert(output[0] == 0 && live == 0);
				checks++;
			}
			failat = 0;
			attempts = 0;
			assert(recordserialize(kind, &sa, "owner\nforged", 1, output, sizeof output) == -1);
			assert(output[0] == 0 && attempts == 0);
			checks++;
		}
	}
}

/* Decimal parsing requires the exact line delimiter and leaves failed cursors untouched. */
static void
expirationparsing(void)
{
	char text[96], *p, *original;
	char *invalid[] = {"", "\n", "xAQ==\n", "1xAQ==\n", "1", "-\n", "+\n",
		" 1\n", "1 \n", "0x1\n", "184467440737095516160\n"};
	long values[] = {0, 1, -1, LONG_MAX, LONG_MIN}, parsed;
	int i;

	for(i = 0; i < nelem(values); i++){
		snprintf(text, sizeof text, "%ld\nAQ==\n", values[i]);
		p = text;
		parsed = 123;
		assert(keyringexpiration(&p, &parsed) && parsed == values[i]);
		assert(strcmp(p, "AQ==\n") == 0);
		checks++;
	}
	for(i = 0; i < nelem(invalid); i++){
		original = p = invalid[i];
		parsed = 123;
		assert(!keyringexpiration(&p, &parsed) && p == original && parsed == 123);
		checks++;
	}
	snprintf(text, sizeof text, "%lu\nAQ==", ULONG_MAX);
	p = text;
	assert(keyringexpiration(&p, &parsed) && (ulong)parsed == ULONG_MAX && strcmp(p, "AQ==") == 0);
	p = "+00012\nnext";
	assert(keyringexpiration(&p, &parsed) && parsed == 12 && strcmp(p, "next") == 0);
	assert(!keyringexpiration(nil, &parsed));
	p = nil;
	assert(!keyringexpiration(&p, &parsed));
	p = "1\n";
	assert(!keyringexpiration(&p, nil));
	checks += 5;
}

/* Model ownership of the three VM header objects around the real certificate parser. */
static int certowned, certsigcalls;
static SigAlg certalgorithm;
static SigAlgVec certvector;
static String certstrings[2];
static Certificate certresult;

/* Header lookup is controlled; each accepted object becomes an independently owned reference. */
static SigAlg*
strtoalg(char *text, char **next)
{
	if(strncmp(text, "rsa\n", 4) != 0)
		return nil;
	*next = text+4;
	certowned++;
	return &certalgorithm;
}

/* Only the well-formed metadata lines used by the expiration tests enter this adapter. */
static String*
strtostring(char *text, char **next)
{
	char *end;
	int index;

	end = strchr(text, '\n');
	assert(end != nil && certowned >= 1 && certowned <= 2);
	index = certowned-1;
	certowned++;
	*next = end+1;
	return &certstrings[index];
}

/* Invalid expiration/signature paths must release exactly the acquired VM references. */
void
destroy(void *object)
{
	assert(object == &certalgorithm || object == &certstrings[0] || object == &certstrings[1]);
	assert(certowned > 0);
	certowned--;
}

/* Observe whether a malformed expiration can reach the signature parser at all. */
static void*
certsignature(char *text, char **next)
{
	certsigcalls++;
	if(strcmp(text, "AQ==\n") != 0)
		return nil;
	*next = text+strlen(text);
	return &values[0];
}

/* Successful construction transfers all three header references into the certificate. */
static Certificate*
newCertificate(SigAlg *sa, String *ha, String *signer, long expiration, int incref)
{
	assert(sa == &certalgorithm && ha == &certstrings[0] && signer == &certstrings[1]);
	assert(certowned == 3 && incref == 0);
	certowned = 0;
	memset(&certresult, 0, sizeof certresult);
	certresult.x.exp = expiration;
	return &certresult;
}

CERTIFICATE_IMPLEMENTATION

/* Check actual caller cleanup and ensure invalid expirations never enter signature parsing. */
static void
certificateparsing(void)
{
	char input[256];
	char *bad[] = {"x", "1x", "\n", "-\n", "+\n", "1 \n",
		" 1\n", "184467440737095516160\n"};
	long good[] = {0, 1, -1, LONG_MAX, LONG_MIN};
	Certificate *result;
	int i;

	memset(&certalgorithm, 0, sizeof certalgorithm);
	memset(&certvector, 0, sizeof certvector);
	certalgorithm.vec = &certvector;
	certvector.str2sig = certsignature;
	for(i = 0; i < nelem(good); i++){
		snprintf(input, sizeof input, "rsa\nsha1\nowner\n%ld\nAQ==\n", good[i]);
		certsigcalls = 0;
		result = strtocert(input);
		assert(result == &certresult && result->x.exp == good[i]);
		assert(result->signa == &values[0] && certowned == 0 && certsigcalls == 1);
		checks++;
	}
	for(i = 0; i < nelem(bad); i++){
		snprintf(input, sizeof input, "rsa\nsha1\nowner\n%sAQ==\n", bad[i]);
		certsigcalls = 0;
		assert(strtocert(input) == H && certowned == 0 && certsigcalls == 0);
		checks++;
	}
	snprintf(input, sizeof input, "rsa\nsha1\nowner\n1\ninvalid");
	certsigcalls = 0;
	assert(strtocert(input) == H && certowned == 0 && certsigcalls == 1);
	checks++;
}

/* Run the full lower-level suites before the complete-record regression matrix. */
int
main(int argc, char **argv)
{
	if(argc == 1 || strcmp(argv[1], "integers") == 0 ||
	   strcmp(argv[1], "failure") == 0 || strcmp(argv[1], "bounds") == 0 ||
	   strcmp(argv[1], "fields") == 0 || strcmp(argv[1], "field-failure") == 0)
		fields_main(argc, argv);
	fieldsetup();
	fmtinstall('U', big64conv);
	checks = 0;
	if(argc == 1 || strcmp(argv[1], "records") == 0)
		recordbounds();
	if(argc == 1 || strcmp(argv[1], "record-failure") == 0)
		recordfailures();
	if(argc == 1 || strcmp(argv[1], "record-faults") == 0)
		recordfaults();
	if(argc == 1 || strcmp(argv[1], "expiration") == 0)
		expirationparsing();
	if(argc == 1 || strcmp(argv[1], "certificates") == 0)
		certificateparsing();
	printf("Keyring records: %d capacity/header/expiration cases passed\n", checks);
	return 0;
}
