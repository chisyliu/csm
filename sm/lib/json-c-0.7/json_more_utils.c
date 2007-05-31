#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include "debug.h"
#include "bits.h"
#include "json_object.h"
#include "json_more_utils.h" 
#include "json_tokener.h"
#include "JSON_checker.h"
#include "linkhash.h"

int json_stream_skip(FILE*f) {
	JSON_checker_init();
	int count = 0;
	while(1) {
		char c;
		if(1 != fread(&c,1,1,f)) {
			if(feof(f)) {
				if(count==0) return 0;
				mc_error("EOF after %d bytes were read.\n", count);
				return 0;
			} 
			mc_error("Reading error: %s\n", strerror(errno));
			return 0;
		} else {
			if(!JSON_checker_push(c)) {
				mc_error("Malformed JSON object. (read %d bytes).\n", count);
				return 0;
			}
		
			if(JSON_checker_finished())
				return 1;
		}
		count++;
	}
}

JO json_read_stream(FILE*f) {
	#define MAX_SIZE 500000
	char buf[MAX_SIZE+1];
	unsigned short bufs[MAX_SIZE+1];
	
	int count = 0;
	
	JSON_checker_init();
	while(count < MAX_SIZE) {
		char c;
		if(1 != fread(&c,1,1,f)) {
			if(feof(f)) {
				if(count==0) return 0;
				mc_error("EOF while %d were read: \n\t'%.*s'. \n", count, count, buf);
				return 0;
			} 
			mc_error("Reading error: %s\n", strerror(errno));
			return 0;
		} else {
			if(count==0 && isspace(c)) continue;
			
			buf[count] = c;
			bufs[count] = c;
			count++;

			if(!JSON_checker_push(c)) {
				mc_error("Malformed JSON object: \n'%.*s'\n", count, buf);
				return 0;
			}
			
			if(JSON_checker_finished()) {
/*				sm_de("Found object.\n"); */
				JO jo = json_tokener_parse_len(buf, count);

				return jo;
			}
			
		}
	}
	
	mc_error("Object size = %d is bigger than MAX_SIZE = %d\n",  MAX_SIZE);	
	return 0;
}



struct json_object* json_tokener_parse_len(char *str, int len) {
  struct json_tokener* tok;
  struct json_object* obj;

  tok = json_tokener_new();
  obj = json_tokener_parse_ex(tok, str, len);
  if(tok->err != json_tokener_success)
    obj = error_ptr(-tok->err);
  json_tokener_free(tok);
  return obj;
}



int jo_read_double_array(JO s, const char*name, double*p, int n, double when_null) {
	JO jo = json_object_object_get(s, (char*)name);
	if(!jo) {
/*		mc_error("Field '%s' not found.\n", name); */
		return 0;
	}
	
	if(!json_object_is_type(jo, json_type_array)) {
		mc_error("This is not an array: '%s'\n",json_object_to_json_string(jo));
		return 0;
	}
	int size = json_object_array_length(jo);
	if(size < n) {
		mc_error("I expected at least %d elements, got %d. \nArray: '%s'\n",
			n, size, json_object_to_json_string(jo));
		return 0;
	}
	int i; for(i=0;i<n;i++) {
		JO v = json_object_array_get_idx(jo, i);
		if(!v)
			p[i] = when_null;
		else
		 	if(json_object_is_type(v, json_type_double)) {
				p[i] = json_object_get_double(v);
			} else
			if(json_object_is_type(v, json_type_int)) {
				p[i] = json_object_get_int(v);
			} else
			p[i] = when_null;
	}
	return 0;
}

int jo_read_int_array(JO s, const char*name, int*p, int n, int when_null) {
	JO jo = json_object_object_get(s, (char*)name);
	if(!jo) {
/*		mc_error("Field '%s' not found.\n", name); */
		return 0;
	}
	if(!json_object_is_type(jo, json_type_array)) {
		mc_error("This is not an array: '%s'\n",json_object_to_json_string(jo));
		return 0;
	}
	int size = json_object_array_length(jo);
	if(size < n) {
		mc_error("I expected at least %d elements, got %d. \nArray: '%s'\n",
			n, size, json_object_to_json_string(jo));
		return 0;
	}
	int i; for(i=0;i<n;i++) {
		JO v = json_object_array_get_idx(jo, i);
		if(!v || !json_object_is_type(v, json_type_int))
			p[i] = when_null;
		else
			p[i] = json_object_get_int(v);
	}
	return 0;
}

JO jo_double_or_null(double v) {
	return (v == v) ?  /* NAN is null */
		json_object_new_double(v) : jo_new_null() ;
}

JO jo_new_double_array(const double *v, int n) {
	JO array = json_object_new_array();
	int i; for(i=0;i<n;i++) {
		json_object_array_add(array, jo_double_or_null(v[i]));
	}
	return array;
}

JO jo_new_int_array(const int *v, int n) {
	JO array = json_object_new_array();
	int i; for(i=0;i<n;i++) {
		json_object_array_add(array, json_object_new_int(v[i]));
	}
	return array;
}


int jo_read_int(JO jo, const char*name, int*p) {
	JO v = json_object_object_get(jo, (char*)name);
	
	if(!v) {
/*		mc_error("Field '%s' not found.\n", name); */
		return 0;
	}
	
	if(!json_object_is_type(v, json_type_int)) {
		mc_error("I was looking for a int, instead got '%s'.\n",
		         json_object_to_json_string(v));
		return 0;
	}
	
	*p = json_object_get_int(v);
	return 1;
}

double convert_to_double(JO jo) {
	if(json_object_is_type(jo, json_type_double)) 
		return json_object_get_double(jo);
	else 
		return NAN;
}

int jo_read_double(JO jo, const char*name, double*p) {
	JO v = json_object_object_get(jo, (char*)name);
	
	if(!v) {
/*		mc_error("Field '%s' not found.\n", name); */
		return 0;
	}
	
	*p = convert_to_double(v);
	return 1;
}

void jo_add_double_array(JO root, const char*name, const double*v, int n) {
	jo_add(root, name, jo_new_double_array(v, n));
}

void jo_add_int_array(JO root, const char*name, const int*v, int n) {
	jo_add(root, name, jo_new_int_array(v, n));
}

void jo_add_int(JO root, const char*name, int v) {
	jo_add(root, name, jo_new_int(v));
}

void jo_add_double(JO root, const char*name, double v) {
	jo_add(root, name, jo_new_double(v));
}

JO find_object_with_name(JO root, const char*name) {
	json_object_object_foreach(root, key, val) {
		if(!strcmp(key, name)) return root;
		if(json_object_is_type(val, json_type_object)) {
			JO jo = find_object_with_name(val, name);
			if(jo) return jo;
		}
	}
	return 0;
}

