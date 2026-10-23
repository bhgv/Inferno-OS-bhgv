/* Field serialization fixtures reuse the integer codec and allocation observers above. */
static mpint values[8];
static mpdigit valuedigits[8];
static RSApriv rsakey;
static DSApriv dsakey;
static DSAsig dsasignature;
static EGpriv egkey;
static EGsig egsignature;
static char *texts[] = {"AQ==", "AIE=", "AP8=", "fw==", "AA==", "EA==", "AIQ=", "Kw=="};
static int fieldcounts[] = {8, 2, 1, 5, 4, 2, 4, 3, 2};

/* Give every algorithm field a distinct real native magnitude. */
static void
fieldsetup(void)
{
	int i;
	mpdigit digits[] = {1, 129, 255, 127, 0, 16, 132, 43};

	memset(values, 0, sizeof values);
	for(i = 0; i < 8; i++){
		valuedigits[i] = digits[i];
		values[i].p = &valuedigits[i];
		values[i].top = digits[i] != 0;
		values[i].size = 1;
		values[i].sign = 1;
	}
	rsakey.pub.n = &values[0]; rsakey.pub.ek = &values[1]; rsakey.dk = &values[2];
	rsakey.p = &values[3]; rsakey.q = &values[4]; rsakey.kp = &values[5];
	rsakey.kq = &values[6]; rsakey.c2 = &values[7];
	dsakey.pub.p = &values[0]; dsakey.pub.q = &values[1]; dsakey.pub.alpha = &values[2];
	dsakey.pub.key = &values[3]; dsakey.secret = &values[4];
	dsasignature.r = &values[0]; dsasignature.s = &values[1];
	egkey.pub.p = &values[0]; egkey.pub.alpha = &values[1]; egkey.pub.key = &values[2];
	egkey.secret = &values[3]; egsignature.r = &values[0]; egsignature.s = &values[1];
}

/* Invoke all nine actual native key/signature serializers. */
static int
fieldserialize(int op, char *buf, int len)
{
	switch(op){
	case 0: return rsa_sk2str(&rsakey, buf, len);
	case 1: return rsa_pk2str(&rsakey.pub, buf, len);
	case 2: return rsa_sig2str(&values[0], buf, len);
	case 3: return dsa_sk2str(&dsakey, buf, len);
	case 4: return dsa_pk2str(&dsakey.pub, buf, len);
	case 5: return dsa_sig2str(&dsasignature, buf, len);
	case 6: return eg_sk2str(&egkey, buf, len);
	case 7: return eg_pk2str(&egkey.pub, buf, len);
	default: return eg_sig2str(&egsignature, buf, len);
	}
}

/* Require exact field order/newlines at every buffer capacity, with no partial success. */
static void
fieldbounds(void)
{
	char expected[256], output[260];
	int op, i, need, cap, n;

	for(op = 0; op < 9; op++){
		expected[0] = 0;
		for(i = 0; i < fieldcounts[op]; i++){
			strcat(expected, texts[i]);
			strcat(expected, "\n");
		}
		need = strlen(expected);
		for(cap = 0; cap <= need+2; cap++){
			memset(output, 0xa5, sizeof output);
			n = fieldserialize(op, output+1, cap);
			if(cap <= need){
				assert(n == -1);
				if(cap > 0)
					assert(output[1] == 0);
			}else
				assert(n == need && strcmp(output+1, expected) == 0);
			assert((uchar)output[0] == 0xa5 && (uchar)output[cap+1] == 0xa5 && live == 0);
			checks++;
		}
	}
}

/* Every field export failure must clear the record and release its temporary buffer. */
static void
fieldfailures(void)
{
	char output[256];
	int op, field;

	for(op = 0; op < 9; op++)
	for(field = 1; field <= fieldcounts[op]; field++){
		attempts = 0;
		failat = field;
		memset(output, 0xa5, sizeof output);
		assert(fieldserialize(op, output, sizeof output) == -1 && output[0] == 0 && live == 0);
		checks++;
	}
	failat = 0;
	rsakey.pub.n = nil;
	assert(rsa_pk2str(&rsakey.pub, output, sizeof output) == -1 && output[0] == 0);
	assert(keyringfields(nil, 0, nil, 1) == -1);
	assert(keyringfields(output, sizeof output, nil, 1) == -1 && output[0] == 0);
	checks += 3;
	fieldsetup();
}

/* Run native codec/Fmt tests before complete-key capacity and field fault coverage. */
int
main(int argc, char **argv)
{
	if(argc == 1 || strcmp(argv[1], "integers") == 0 ||
	   strcmp(argv[1], "failure") == 0 || strcmp(argv[1], "bounds") == 0)
		integer_main(argc, argv);
	fmtinstall('U', big64conv);
	checks = 0;
	fieldsetup();
	if(argc == 1 || strcmp(argv[1], "fields") == 0)
		fieldbounds();
	if(argc == 1 || strcmp(argv[1], "field-failure") == 0)
		fieldfailures();
	printf("Keyring fields: %d capacity/order/failure cases passed\n", checks);
	return 0;
}
