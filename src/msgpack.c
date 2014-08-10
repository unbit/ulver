#include <ulver.h>
/*

serialize/deserialize ulver objects and forms in/to msgpack blobs

NOTE: not all objects can be serialized
	
*/

static void msgpack_fix(ulver_msgpack *um, uint64_t amount) {
	if (um->pos + amount <= um->len) return;
	char *new_base = um->env->alloc(um->env, um->len + 4096);
	memcpy(new_base, um->base, um->len);
	um->env->free(um->env, um->base, um->len);
	um->base = new_base;
	um->buf = um->base + um->pos;
	um->len += 4096;
}

static void msgpack_string(ulver_msgpack *um, char *str, uint64_t len) {
	if (len <= 31) {
		msgpack_fix(um, 1 + len);
		*um->buf = (char )((uint8_t) (0xa0 | len)); um->buf++;
		um->pos++;
	}
	else if (len <= 0xff) {
		msgpack_fix(um, 2 + len);
		*um->buf = 0xD9; um->buf++;
		*um->buf = (uint8_t) len; um->buf++;
		um->pos += 2;
	}
	else if (len <= 0xffff) {
		msgpack_fix(um, 3 + len);
		*um->buf = 0xDA; um->buf++;
		*um->buf = (char) ((uint8_t) (len & 0xff)); um->buf++;
                *um->buf = (char) ( (uint8_t) ((len >> 8) & 0xff) ); um->buf++;
		um->pos += 3;
	}
	else {
		msgpack_fix(um, 5 + len);
		*um->buf = 0xDA; um->buf++;
		*um->buf = (char) ((uint8_t) (len & 0xff)); um->buf++;
                *um->buf = (char) ( (uint8_t) ((len >> 8) & 0xff) ); um->buf++;
                *um->buf = (char) ( (uint8_t) ((len >> 16) & 0xff) ); um->buf++;
                *um->buf = (char) ( (uint8_t) ((len >> 24) & 0xff) ); um->buf++;
		um->pos += 5;
	}

	memcpy(um->buf, str, len);
	um->buf+=len;
	um->pos+=len;
}

static void msgpack_bin(ulver_msgpack *um, char *str, uint64_t len) {
        if (len <= 0xff) {
                msgpack_fix(um, 2 + len);
                *um->buf = 0xC4; um->buf++;
                *um->buf = (uint8_t) len; um->buf++;
                um->pos += 2;
        }
        else if (len <= 0xffff) {
                msgpack_fix(um, 3 + len);
                *um->buf = 0xC5; um->buf++;
                *um->buf = (char) ((uint8_t) (len & 0xff)); um->buf++;
                *um->buf = (char) ( (uint8_t) ((len >> 8) & 0xff) ); um->buf++;
                um->pos += 3;
        }
        else {
                msgpack_fix(um, 5 + len);
                *um->buf = 0xC6; um->buf++;
                *um->buf = (char) ((uint8_t) (len & 0xff)); um->buf++;
                *um->buf = (char) ( (uint8_t) ((len >> 8) & 0xff) ); um->buf++;
                *um->buf = (char) ( (uint8_t) ((len >> 16) & 0xff) ); um->buf++;
                *um->buf = (char) ( (uint8_t) ((len >> 24) & 0xff) ); um->buf++;
                um->pos += 5;
        }

        memcpy(um->buf, str, len);
	um->buf+=len;
        um->pos+=len;
}

static void msgpack_num(ulver_msgpack *um, int64_t num) {
	if (num > 0 && num <= 127) {
		msgpack_fix(um, 1);
		*um->buf = (char )((uint8_t) num);
		um->buf++;
		um->pos++;
		return;
	}

	if (num < 0 && num >= 31) {
		msgpack_fix(um, 1);
                *um->buf = (char )(224 | (int8_t) num);
		um->buf++;
                um->pos++;
                return;
	}

	if (num <= 127 && num >= -127) {
		msgpack_fix(um, 2);
		*um->buf = 0xD0;
		um->buf++;
		*um->buf = (int8_t) num;
		um->buf++;
		um->pos+=2;
		return;
	}
	else if (num <= 32767 && num >= -32767) {
		msgpack_fix(um, 3);
		*um->buf = 0xD1; um->buf++;
		*um->buf = (char) ((uint8_t) (num & 0xff)); um->buf++;
		*um->buf = (char) ( (uint8_t) ((num >> 8) & 0xff) ); um->buf++;
		um->pos+=3;
		return;
	}
	else if (num <= 2147483647LL && num >= -2147483648LL) {
		msgpack_fix(um, 5);
		*um->buf = 0xD2; um->buf++;
		*um->buf = (char) ((uint8_t) (num & 0xff)); um->buf++;
                *um->buf = (char) ( (uint8_t) ((num >> 8) & 0xff) ); um->buf++;
                *um->buf = (char) ( (uint8_t) ((num >> 16) & 0xff) ); um->buf++;
                *um->buf = (char) ( (uint8_t) ((num >> 24) & 0xff) ); um->buf++;
                um->pos+=5;
                return;
	}

	msgpack_fix(um, 9);
	*um->buf = 0xD2; um->buf++;
        *um->buf = (char) ((uint8_t) (num & 0xff)); um->buf++;
        *um->buf = (char) ( (uint8_t) ((num >> 8) & 0xff) ); um->buf++;
        *um->buf = (char) ( (uint8_t) ((num >> 16) & 0xff) ); um->buf++;
        *um->buf = (char) ( (uint8_t) ((num >> 24) & 0xff) ); um->buf++;
        *um->buf = (char) ( (uint8_t) ((num >> 32) & 0xff) ); um->buf++;
        *um->buf = (char) ( (uint8_t) ((num >> 40) & 0xff) ); um->buf++;
        *um->buf = (char) ( (uint8_t) ((num >> 48) & 0xff) ); um->buf++;
        *um->buf = (char) ( (uint8_t) ((num >> 56) & 0xff) ); um->buf++;
        um->pos+= 9;
}

static void msgpack_array(ulver_msgpack *um, uint64_t count) {

	if (count <= 15) {
		msgpack_fix(um, 1);
                *um->buf = (char )((uint8_t) (0x90 | count)); um->buf++;
                um->pos++;		
		return;
	}

	if (count <= 0xffff) {
		msgpack_fix(um, 3);
                *um->buf = 0xDC; um->buf++;
                *um->buf = (char) ((uint8_t) (count & 0xff)); um->buf++;
                *um->buf = (char) ( (uint8_t) ((count >> 8) & 0xff) ); um->buf++;
                um->pos += 3;
		return;
	}

	msgpack_fix(um, 5);
        *um->buf = 0xDD; um->buf++;
        *um->buf = (char) ((uint8_t) (count & 0xff)); um->buf++;
        *um->buf = (char) ( (uint8_t) ((count >> 8) & 0xff) ); um->buf++;
        *um->buf = (char) ( (uint8_t) ((count >> 16) & 0xff) ); um->buf++;
        *um->buf = (char) ( (uint8_t) ((count >> 24) & 0xff) ); um->buf++;
        um->pos += 5;
}

static uint64_t array_form(ulver_form *of) {
        uint64_t count = 0;
        while(of) {
                count++;
                of = of->next;
        }
        return count;
}

ulver_msgpack *ulver_form_serialize(ulver_env *env, ulver_form *uf, ulver_msgpack *um) {
	if (!um) {
		um = env->alloc(env, sizeof(ulver_msgpack));
		um->len = 4096;
		um->base = um->buf = env->alloc(env, um->len);
	}

	switch(uf->type) {
		case ULVER_LIST:
			msgpack_array(um, array_form(uf->list));
			ulver_form *item = uf->list;
			while(item) {
				ulver_form_serialize(env, item, um);
				item = item->next;
			}
			break;
		case ULVER_SYMBOL:
			msgpack_string(um, uf->value, uf->len);
			break;
		case ULVER_NUM:
			msgpack_num(um, strtoll(uf->value, NULL, 10));
			break;
		case ULVER_FLOAT:
			break;
		case ULVER_STRING:
			msgpack_bin(um, uf->value, uf->len);
			break;
		case ULVER_KEYWORD:
			msgpack_string(um, uf->value, uf->len);
			break;
		default:
			printf("unserializable form\n");
			return um;
	}

	return um;
}

static uint64_t array_object_item(ulver_object_item *oi) {
	uint64_t count = 0;
        while(oi) {
                count++;
                oi = oi->next;
        }
	return count;
}

ulver_msgpack *ulver_object_serialize(ulver_env *env, ulver_object *uo, ulver_msgpack *um) {
	if (!um) {
                um = env->alloc(env, sizeof(ulver_msgpack));
                um->len = 4096;
                um->base = um->buf = env->alloc(env, um->len);
        }

        switch(uo->type) {
                case ULVER_LIST:
                        msgpack_array(um, array_object_item(uo->list));
                        ulver_object_item *item = uo->list;
                        while(item) {
                                ulver_object_serialize(env, item->o, um);
                                item = item->next;
                        }
                        break;
                case ULVER_NUM:
                        msgpack_num(um, uo->n);
                        break;
                case ULVER_FLOAT:
                        break;
                case ULVER_STRING:
                        msgpack_bin(um, uo->str, uo->len);
                        break;
                default:
                        printf("unserializable object\n");
                        return um;
        }

        return um;
}

ulver_form *ulver_form_deserialize(ulver_env *env, ulver_form *parent, char **buf, uint64_t *len) {
	if (*len == 0) return NULL;
	uint8_t *ptr = (uint8_t *) *buf;

	// tinynum ?
	if (*ptr <= 127) {
		ulver_form *uf = ulver_form_push_form(env, parent, ULVER_NUM);
		uf->value = ulver_util_str2num(env, *ptr, &uf->len);
		uf->need_free = 21;
		*buf+=1;
		*len-=1;
		return uf;
	}

	// array ?
	if (*ptr >= 0x90 && *ptr <= 0x9f) {
		uint8_t count = *ptr & 0x0f;
		uint8_t i;
		ulver_form *array = ulver_form_push_form(env, parent, ULVER_LIST);
		*buf+=1;
		*len-=1;
		if (*len < count) return NULL;
		for(i=0;i<count;i++) {
			if (*len > 0) {
				ulver_form_deserialize(env, array, buf, len);
				continue;
			}
			return NULL;
		}
		return array;
	}

	// string ? (symbol or keyword)
	if (*ptr >= 0xa0 && *ptr <= 0xbf) {
		ulver_form *str = ulver_form_push_form(env, parent, ULVER_SYMBOL);
		uint8_t count = *ptr & 0x1f;
		*buf+=1;
                *len-=1;
		// is it a keyword ?
		if (count > 0 && **buf == ':') {
			str->type = ULVER_KEYWORD;
		}
		if (*len < count) return NULL;
		str->value = ulver_utils_strndup(env, *buf, count);
		str->len = count;
		str->need_free = count+1;
		*buf += count;
		*len -= count;
		return str;
	}

	*buf+=1;
        *len-=1;
	ulver_form *uf = NULL;

	if (*len == 0) return NULL;

	uint8_t h = *ptr;
	ptr+=1;

	switch(h) {
		// bin8
		case 0xC4:
			uf = ulver_form_push_form(env, parent, ULVER_STRING);	
			uint8_t count = *ptr;		
			*buf+=1;
        		*len-=1;	
			if (*len < count) return NULL;
			uf->value = ulver_utils_strndup(env, *buf, count);
                	uf->len = count;
                	uf->need_free = count+1;
                	*buf += count;
                	*len -= count;
			break;
		default:
			printf("unknown msgpack code %x\n", h);
			break;
	}

	return uf;
}
