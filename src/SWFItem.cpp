#include "SWFItem.h"
#include <cstring>
#include <cctype>
#include "base64.h"
#include <sys/types.h>
#include <vector>
#include "XmlAutoPtr.h"

using namespace std;

namespace SWF {

	// ------------ utility functions

	int swf_get_bits_needed_for_uint (uint64_t value) {
		int i=0;
		while (value > 0) {
			value >>= 1;
			i++;
		}
		return i;
	}

	int swf_get_bits_needed_for_int( int64_t value ) {
		if (value < 0) {
			return swf_get_bits_needed_for_uint(~value) + 1;
		} else if (value > 0) {
			return swf_get_bits_needed_for_uint(value) + 1;
		} else {
			// The SWF Specification is a bit vague about whether we require
			// any bits to represent zero as a signed integer, but Flash
			// Player appears to fall over sometimes if we set the size to
			// zero so let's use one bit to be safe.
			return 1;
		}
	}

	int swf_get_bits_needed_for_fp(double value, int exp = 16) {
		return swf_get_bits_needed_for_int((int64_t)(value * (1 << exp)));
	}

	int SWFBitsNeeded(float value, int exp, bool is_signed) {
		if (!is_signed) {
			fprintf(stderr, "FIXME: calculate bits for unsigned float\n");
		}

		return swf_get_bits_needed_for_fp(value, exp);
	}

	int SWFBitsNeeded(int32_t value, bool is_signed) {
		if (is_signed) {
			return swf_get_bits_needed_for_int(value);
		} else {
			return swf_get_bits_needed_for_uint(value);
		}
	}

	long SWFMaxBitsNeeded(bool is_signed, int how_many, ...) {
		long bits = 0;
		va_list ap;
		int n;
		va_start(ap, how_many);

		for (int i=0; i<how_many; i++) {
			int b = SWFBitsNeeded(va_arg(ap, int), is_signed);
			if (b > bits) {
				bits = b;
			}
		}

		va_end(ap);
		return bits;
	}

	Item::Item() {
		file_offset = -1;
		cached_size = -1;
	}

	size_t Item::getSize(Context *ctx, int start_at) {
		if(cached_size == -1) {
			cached_size = calcSize(ctx, start_at);
		}
		return cached_size;
	}

	int Item::getHeaderSize(int size) {
		return 0;
	}

	void Item::writeHeader(Writer *w, Context *ctx, size_t len) {
	}


	Rest::Rest() {
		size = 0;
	}

	bool Rest::parse(Reader *r, int end, Context *ctx) {
		file_offset = r->getPosition();

		size = end - r->getPosition();
		if (size > 0) {
			data = vector<unsigned char>(size);
			r->getData(&data[0], size);
		}

		return r->getError() == SWFR_OK;
	}

	void Rest::dump(int indent, Context *ctx) {
		for( int i=0; i<indent; i++ ) printf("  ");
		printf("Rest (length %i)\n", size);
		if (size) {
			int i=0;
			while (i<size) {
				for (int in=0; in<indent+1; in++) {
					printf("  ");
				}
				for (int n=0; n<8 && i<size; n++) {
					printf(" %02X",  data[i]);
					i++;
				}
				printf("\n");
			}
		}
	}

	size_t Rest::calcSize(Context *ctx, int start_at) {
		int r = start_at;
		r += size * 8;
		r += Item::getHeaderSize(r-start_at);
		return r-start_at;
	}

	void Rest::write(Writer *w, Context *ctx) {
		Item::writeHeader(w, ctx, 0);

		if (size) {
			w->putData(&data[0], size);
		}
	}

	#define TMP_STRLEN 0xFF
	void Rest::writeXML(xmlNodePtr xml, Context *ctx) {
		char tmp[TMP_STRLEN];
		xmlNodePtr node = xml;

		{
			if (size) {
				char *tmp_data = (char *)&data[0];
				int sz = size;
				vector<char> tmpstr(sz * 3);

				int l = base64_encode(&tmpstr[0], tmp_data, sz);
				if (l > 0) {
					tmpstr[l] = 0;
					xmlNewTextChild(node, NULL, (const xmlChar *)"data", (const xmlChar *)&tmpstr[0]);
				}
			}
		}
	}

	void Rest::parseXML(xmlNodePtr node, Context *ctx) {
		data.clear();
		size = 0;

		XmlCharAutoPtr xmld(xmlNodeGetContent(node));
		if (xmld.get()) {
			// unsure if this is neccessary
			int end;
			for (end=strlen((char*)xmld.get()); end>0 && isspace((char)xmld[end-1]); --end) {
				xmld[end-1] = 0;
			}

			int start = 0;
			while (isspace(xmld[start])) {
				++start;
			}

			int length = end - start;

			vector<unsigned char> dst(length);
			int outLength = base64_decode((char*)&dst[0], (char*)xmld.get()+start, length);

			if (outLength > 0) {
				size = outLength;
				data = dst;
			}
		}
	}

	void Rest::getdata(unsigned char **d, int *s) {
		*d = &data[0];
		*s = size;
	}

	void Rest::setdata(unsigned char *d, int s) {
		data.clear();
		size = s;

		if (size) {
			data = vector<unsigned char>(size);
			memcpy(&data[0], d, size);
		}
	}

}
