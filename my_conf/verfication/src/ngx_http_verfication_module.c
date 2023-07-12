/*
 * Author: Weibin Yao(yaoweibin@gmail.com)
 *
 * Licence: This module could be distributed under the
 * same terms as Nginx itself.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>
#include <sys/types.h>
#include <gd.h>
#include <gdfontg.h>
#include <gdfontl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include "ngx_http_security.h"


#define ngx_buffer_init(b) b->pos = b->last = b->start;


typedef struct {
    ngx_str_t      match;
    ngx_str_t      sub;
} sub_pair_t;


typedef struct {
    ngx_hash_t     types;
    ngx_array_t   *types_keys;
    ngx_flag_t     enable;
    ngx_flag_t     case_enable;
} ngx_http_verification_code_loc_conf_t;

static ngx_buf_t * buffer_append_string(ngx_buf_t *b, u_char *s, size_t len,
    ngx_pool_t *pool);

static void *ngx_http_verification_code_create_conf(ngx_conf_t *cf);
static char *ngx_http_verification_code_merge_conf(ngx_conf_t *cf, void *parent,
    void *child);

static ngx_int_t ngx_http_verification_code_filter_init(ngx_conf_t *cf);

static u_char UPSTREAM_CAHCE_DEFAULT_PAGE_INDEX[2048] = "";
static u_char tamper_page_title[] =
"<html>" CRLF
"<head><title>Alert Message</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>During the period of protection , you can try to access <a href=\"";
static u_char tamper_page_foot[] = "\">default index</a></h1></center>\"" CRLF;

#define HTML_HEAD "<html><body>"
#define FORM_DATA_HEAD_HTTP \
"<form action=\"http://"
#define FORM_DATA_HEAD_HTTPS \
"<form action=\"http://"

#define FORM_DATA_FOOT \
"/form.php\" method=\"post\" enctype=\"application/x-www-form-urlencoded\">"\
"<input type=\"text\"  name=\"waf_ver_code_arg\"/>"\
"<br/>"\
"<input type=\"submit\" name=\"submit\" value=\"Submit\"/>"\
"</form>"\
"</body>"\
"</html>"

#define FORM_DATA \
"<form action=\"http://172.15.85.1/form.php\" method=\"post\" enctype=\"application/x-www-form-urlencoded\">"\
"<input type=\"text\"  name=\"waf_ver_code_arg\"/>"\
"<br/>"\
"<input type=\"submit\" name=\"submit\" value=\"Submit\"/>"\
"</form>"\
"</body>"

static u_char png_mem[]="iVBORw0KGgoAAAANSUhEUgAAAHgAAAAeCAIAAABoq03CAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAETElEQVRoge2ZwWsTTRjGn4R1SbZSYwlVSgmmaE6lSC9KKUU8FYw9yCK2iIiChCIi4sEKQpAiUqWUIsVD/wApoSiUIhL1IsFDiQo91CBSejFIke0SYw6Bx8PuVz/T3ex2dpvv8O2PuWRn9pmXZ2Zn3pmESCJg7wn/1wH8XwiMbhGujL53D6EQQiFMTPjQ5efPePQIZ88imUQ0irY2HDuGq1fx4YMP4gDevsXoqCkejSKZxOgoXr/2R1wcuuDIEQIE2N3tprkDhtTOIkl8/NireCZjq5/J+BC8MM5Gv3lDgD097OkhwHzea5epFG/d4uIiSyVWq6xWWSzy8mXTjqUlceUnT8wBGx9nsUhNo6axWGQmw3CYAOfmvAYvjLPRhgXZLLNZArx4ca9CuXOHAE+dElfo6yPAmRmLqulpAuzrExf3SIhN07ufP3H4MKpVfPkCAEePQlFQLqOtzf9F7Ns3dHWhvR1bW4IK0ShqNWgaDhxorNraQiyGSAS/fnkMUxCHzXBxEZUKBgaQTCKZxOAgKhXkcnsSSns7ANTr4godHV4b2PHpEyYmcOIEDh7Evn04dAhnzuDFi91INJ/wp08T4Py8+XN+3uvX3YTlZQLs7xdXGBsjwNlZi6qZGQIcGxNUtttgb950rdCkbmOD4TAVhbpuPtF1KgrDYa6vC0ZsR73O/n4CnhKPUonxOCWJN27w40fqOnWdxSKvX6ckMR5nqSSofPIk5+a4uspKhbUaSyVms4xECPDlS1cKzYyenCTACxf+emjMmslJwYjtGB8nwFSK1aonnbU1qqrF1FNVrq35FOs/GN+3qrpq3MzoVMpixF69Mh3xkYcPCXD/fq6seJXK59nba2F0b68PiWkDP34QYCLhqrGt0e/fE2BXF+v1xqrubgIsFDzE+C+MxCsS8cGIQoGyzHCYmYxFHi3L4jHrOqemODTEzk5K0l9DGIm4UrA12jhi3b5tUWUkvL4ctAyXZZnLyz6oDQ8T4PS0bUfDwyKy6+t/zsaWxQ3WrWo1dnQ0kwYYi7FWE4l7m6kp8yD3/LknnW2MmDXNokrTCDAeF5E9f54Ajx9nLsevX//sIvW6Z6MXFhxcNsrCgkjcBg8emC7ncuIiDSiKrdHGeqooIrKxGAFubDQ+X131bHQ6bZuQGszOEmA67TLURu7fN132MlQ7MRJEy6XD+HrEknQjjdtp9HZ64waLVuUyJYmyzM1N29c2NynLlCSWy+4DNjHuTCSJz57t+t3mGJdKDZvhygqvXTMvlZ4+FZEdHCTAgQEWCqxU+P0783kODXleo419Y2TE4c2REdvp49Cl04okMHjbXLpkK3vliqDmu3fmpG4od+96M9q4A3NcOnM5c4vYLXtqNMmlJaoqEwnKMmWZiQRV1WtWUyzy3Dnz2NnZyXTavM51b7TD7V2AXwT/GbaIwOgWERjdIgKjW0RgdIsIjG4RgdEtIjC6RfwGwFHau/bPCwEAAAAASUVORK5CYII=";

static ngx_command_t  ngx_http_verification_code_filter_commands[] = {

    { ngx_string("verification_code"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_verification_code_loc_conf_t, enable),
      NULL },

    { ngx_string("verification_case_code"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_verification_code_loc_conf_t, case_enable),
      NULL },

    ngx_null_command
};


static ngx_http_module_t  ngx_http_verification_code_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_verification_code_filter_init,             /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_verification_code_create_conf,             /* create location configuration */
    ngx_http_verification_code_merge_conf               /* merge location configuration */
};


ngx_module_t  ngx_http_verification_code_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_verification_code_filter_module_ctx,      /* module context */
    ngx_http_verification_code_filter_commands,         /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

#define WAF_VER_ARG "waf_ver_code_arg"
#define CODE_VER_SUCCESS 1
#define CODE_VER_FAILED  0


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;

extern volatile ngx_cycle_t  *ngx_cycle;

char code_chars[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g',
    'h', 'i', 'j', 'k', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
};

void verify_gen_random_string(char *buff, size_t len)
{
    size_t i;
    #if 0/*only for test auth success*/
    buff[0] = 'A';
    buff[1] = '2';
    buff[2] = '8';
    buff[3] = 'a';
    return ;
    #endif

    for (i = 0; i < len; ++i) {
        buff[i] = code_chars[abs(random())%sizeof(code_chars)];
    }
}
u_char g_png_mem[4096] = {0};
void ngx_http_verification_code_png(ngx_http_request_t *r, ngx_str_t * decoded)
{
    gdImagePtr  im ;
    gdImagePtr  im2;
    FILE       *fp;
    int         brect[8];
    int         i, blue;
    char        font_path[100];
    char        chars[2] = { 0 };
    char        string[40] = { 0 };
    int         width = 120;
    int         height = 30;
    int         len = 4;
    int         size_out;
    int         len_count = 3;
    char       *data = NULL;
    ngx_str_t   sub;

    if (r->connection->has_verification_code == 1){
        return;
    }
    srandom(time(NULL));
    
    verify_gen_random_string(string, 4);

    strcpy(font_path, "/SE/DejaVuSans.ttf");

    #if 0   
    im2 = gdImageCreateTrueColor(width, height);
    gdImageFilledRectangle(im2, 0, 0, width-1, height-1, 0x00FFFFFF);
    #else    
    im = gdImageCreateTrueColor(width, height);
    gdImageFilledRectangle(im, 0, 0, width-1, height-1, 0x00FFFFFF);
    #endif 

    blue = gdImageColorAllocate(im, 0, 0, 255);

    for (i = 0; i < len; ++i) {
        chars[0] = string[i];

        /* lower left */
        brect[0] = height * i;
        brect[1] = height *(i+1);

        /* lower right */
        brect[2] = height * i;
        brect[3] = height * (i+1);

        /* upper right */
        brect[4] = height * (i+1);
        brect[5] = height *i;

        /* upper left */
        brect[6] = height * i;
        brect[7] = height * i;


        if (gdImageStringFT(im, brect, blue, font_path, 18, 0, height *( i + 0.2), height * 0.7, chars)) {
            printf("failed\n");
            return ;
        }
    }

    #if 0
    for (i = 0; i < len; ++i) {
        /* -40 degree to  +40 degree */
        angle = random()%82 - 41;
        gdImageCopyRotated(im2, im, height*i +half_height,
                half_height, height*i, 0, height, height, angle);
    }

    add_interfering_line(im2, height, width, 3);
    add_hot_pixel(im2, height, width, 0.01f);
    #endif

    fp = fopen("/SE/b.png", "wb");
    if (!fp) {
        fprintf(stderr, "Can't save png image.\n");
        gdImageDestroy(im);
        return ;
    }

#if 0
    gdImagePng(im2, fp);
#else
    gdImagePng(im, fp);
#endif

    fclose(fp);

#if 0
    data = gdImagePngPtr(im2, &size_out);
#else
    data = gdImagePngPtr(im, &size_out);
#endif

    if (!data) {
        /* Error */
        printf("get png error \r\n");
        return;
    }

    sub.data = (u_char *)data;
    sub.len = size_out;

#if 0    
    gdImageDestroy(im2);    
#else    
    gdImageDestroy(im);    
#endif

    r->connection->has_verification_code = 1;

    len = ngx_max(ngx_base64_decoded_length(sub.len), 20);
    decoded->data = g_png_mem;
    decoded->len = len;

    if ( !r->connection->ver_code_string.data ){        
        r->connection->ver_code_string.data = ngx_pnalloc(r->connection->pool, 4);
        if (r->connection->ver_code_string.data == NULL) {
            return ;
        }
    }

    r->connection->ver_code_string.data[0] = string[0];
    r->connection->ver_code_string.data[1] = string[1];
    r->connection->ver_code_string.data[2] = string[2];
    r->connection->ver_code_string.data[3] = string[3];

    ngx_encode_base64(decoded, &sub);

    gdFree(data);
    data = NULL;

    return ;
}

static ngx_buf_t *
buffer_append_string(ngx_buf_t *b, u_char *s, size_t len, ngx_pool_t *pool)
{
    u_char     *p;
    ngx_uint_t capacity, size;

    if (len > (size_t) (b->end - b->last)) {

        size = b->last - b->pos;

        capacity = b->end - b->start;
        capacity <<= 1;

        if (capacity < (size + len)) {
            capacity = size + len;
        }

        p = ngx_palloc(pool, capacity);
        if (p == NULL) {
            return NULL;
        }

        b->last = ngx_copy(p, b->pos, size);

        b->start = b->pos = p;
        b->end = p + capacity;
    }

    b->last = ngx_copy(b->last, s, len);

    return b;
}

static void *
ngx_http_verification_code_create_conf(ngx_conf_t *cf)
{
    ngx_http_verification_code_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_verification_code_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    conf->enable = NGX_CONF_UNSET;
    conf->case_enable = NGX_CONF_UNSET;

    return conf;
}


static char *
ngx_http_verification_code_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_verification_code_loc_conf_t *prev = parent;
    ngx_http_verification_code_loc_conf_t *conf = child;

    if (ngx_http_merge_types(cf, &conf->types_keys, &conf->types,
                             &prev->types_keys, &prev->types,
                             ngx_http_html_default_types)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_value(conf->case_enable, prev->case_enable, 0);

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_verification_code_handler(ngx_http_request_t *r)
{
    int                           i    = 0;
    const apr_array_header_t     *arr  = NULL;
    const apr_table_entry_t      *te   = NULL;
    ngx_http_verification_code_loc_conf_t  *slcf;

    if (!r->arguments) {
        return NGX_DECLINED;
    }

    if (r->connection->ver_code_string_result == CODE_VER_SUCCESS){
        return NGX_DECLINED;
    }

    if (r->connection->ver_code_already_flag == CODE_VER_ALREADY_AUTH){
        return NGX_DECLINED;
    }

    slcf = ngx_http_get_module_loc_conf(r, ngx_http_verification_code_filter_module);

    if (slcf->enable == 0){
        return NGX_DECLINED;
    }

    r->connection->ver_code_string_result = CODE_VER_FAILED;

    arr = apr_table_elts(r->arguments);
    te = (apr_table_entry_t *)arr->elts;
    for (i = 0; i < arr->nelts; i++) {
        msc_arg *arg = (msc_arg *)te[i].val;
        if ((ngx_rstrncmp((u_char *)arg->name, (u_char *)WAF_VER_ARG, arg->name_len) == 0) && 
            (arg->name_len == sizeof(WAF_VER_ARG) - 1 )){
            if (arg->value_len != 4){
                return NGX_DECLINED;
            }
            if (r->connection->ver_code_string.data){
                if (slcf->case_enable == 0){
                    if (strncmp((char *)r->connection->ver_code_string.data, (char *)arg->value, 4) == 0){
                        r->connection->ver_code_string_result = CODE_VER_SUCCESS;
                        LogMessage(TOPWAF_LOG_INFO, DEBUG_PROXY,"auth verification code %c%c%c%c success.",
                            r->connection->ver_code_string.data[0],
                            r->connection->ver_code_string.data[1],
                            r->connection->ver_code_string.data[2],
                            r->connection->ver_code_string.data[3]);
                    }else{
                        LogMessage(TOPWAF_LOG_INFO, DEBUG_PROXY,"auth verification code %c%c%c%c failed.",
                            r->connection->ver_code_string.data[0],
                            r->connection->ver_code_string.data[1],
                            r->connection->ver_code_string.data[2],
                            r->connection->ver_code_string.data[3]);
                    }
                }else{
                    if (strncasecmp((char *)r->connection->ver_code_string.data, (char *)arg->value, 4) == 0){
                        r->connection->ver_code_string_result = CODE_VER_SUCCESS;
                        LogMessage(TOPWAF_LOG_INFO, DEBUG_PROXY,"auth verification code %c%c%c%c success.",
                            r->connection->ver_code_string.data[0],
                            r->connection->ver_code_string.data[1],
                            r->connection->ver_code_string.data[2],
                            r->connection->ver_code_string.data[3]);
                    }else{
                        LogMessage(TOPWAF_LOG_INFO, DEBUG_PROXY,"auth verification code %c%c%c%c failed.",
                            r->connection->ver_code_string.data[0],
                            r->connection->ver_code_string.data[1],
                            r->connection->ver_code_string.data[2],
                            r->connection->ver_code_string.data[3]);
                    }  
                }
            }
        }        
    }

    return NGX_DECLINED;
}

void my_itoa(uint16_t n,u_char *out_buf)
{
    char s[128];
    int i,j,sign;

    if((sign=n)<0)
        n=-n;
    i=0;
    do{
        s[i++]=n%10+'0';
    }while((n/=10)>0);

    if(sign<0)
        s[i++]='-';
    s[i]='\0';
    for(j=i-1;j>=0;j--)
        *out_buf++=s[j];
    *out_buf='\0';
}

u_char g_ngx_http_ver_code_b[4096] = {0};

ngx_int_t
ngx_http_verification_code_send_png(ngx_http_request_t *r, ngx_int_t first_accuss)
{    
    ngx_int_t rc;
    ngx_chain_t out;
    char *schema = NULL;
    ngx_buf_t * b = NULL;
    ngx_table_elt_t             *h = NULL;
    ngx_list_part_t             *part = NULL;
    unsigned int i = 0;
    u_char *p = NULL;
    u_char *host = NULL;
    u_char       img_buf_head[] = "<img src=\"data:image/png;base64,";
    size_t       img_buf_head_len = sizeof(img_buf_head) - 1;
    u_char       img_buf_foot[] = "\">";
    size_t       img_buf_foot_len = sizeof(img_buf_foot) - 1;
    ngx_str_t    decoded = {0};
    struct       sockaddr *dst_sockaddr = NULL;
    struct       sockaddr_in *sin ;      
    char         str[INET_ADDRSTRLEN] = {0};
    u_char       port[128] = {0};
    
    r->headers_out.content_type.len = sizeof("text/html") - 1;
    r->headers_out.content_type.data = (u_char *) "text/html";
    r->headers_out.status = NGX_HTTP_OK;  
    
    memset(&out, 0, sizeof(out));

    b = ngx_palloc(r->pool, sizeof(ngx_buf_t));
    if(b == NULL){
        return NGX_DECLINED;
    }
    b->last = b->start = b->pos;
    //b->last = b->start = b->pos = g_ngx_http_ver_code_b;
    //b->end = g_ngx_http_ver_code_b + 4095;

    out.buf = b;    
    out.next = NULL;

    if (!r->connection->ssl) {
        schema = "http://";
    } else {
        schema = "https://";
    }
    part = &r->headers_in.headers.part;
    h = part->elts;

    for (i = 0; i < part->nelts; i++){
        if (strcasecmp((const char *)h[i].key.data, "host") == 0) {
            host = (u_char *)h[i].value.data;
        }

        if (host != NULL){
            break;
        }
    }
    if (host == NULL){
        return NGX_DECLINED;
    }

    ngx_str_t keepalive_name = ngx_string("Keep-Alive");
    ngx_str_t keepalive_value = ngx_string("timeout=10, max=100");
    h = ngx_list_push(&r->headers_out.headers);
    if(h == NULL){
        return NGX_DECLINED;
    }
    h->hash= 1;
    h->key.len= keepalive_name.len;
    h->key.data= keepalive_name.data;
    h->value.len= keepalive_value.len;
    h->value.data= keepalive_value.data;

    dst_sockaddr = r->connection->local_sockaddr;
    sin  = (struct sockaddr_in *)dst_sockaddr;
    /*printf("ngx_http_verification_code_match_fix_verification_codetituion ipv4....dst:%s,,%d \r\n",
    inet_ntop(AF_INET,&sin->sin_addr,str,sizeof(str)),port);*/

    if (buffer_append_string(out.buf, (u_char *)HTML_HEAD, sizeof(HTML_HEAD) - 1,
                             r->pool) == NULL) {
        return NGX_ERROR;
    }  

    if (buffer_append_string(out.buf, img_buf_head, img_buf_head_len,
                             r->pool) == NULL) {
        return NGX_ERROR;
    }        

    //decoded.data = png_mem;
    //decoded.len = 1572;

    ngx_http_verification_code_png(r, &decoded);

    if (decoded.len == 0){
        return NGX_ERROR;
    }
    if (buffer_append_string(out.buf, decoded.data, decoded.len,
                             r->pool) == NULL) {
        return NGX_ERROR;
    }

    if (buffer_append_string(out.buf, img_buf_foot, img_buf_foot_len,
                             r->pool) == NULL) {
        return NGX_ERROR;
    }

    if (!r->connection->ssl) {
        if (buffer_append_string(out.buf, (u_char *)FORM_DATA_HEAD_HTTP, sizeof(FORM_DATA_HEAD_HTTP) - 1,
                     r->pool) == NULL) {
            return NGX_ERROR;
        }
    } else {
        if (buffer_append_string(out.buf, (u_char *)FORM_DATA_HEAD_HTTPS, sizeof(FORM_DATA_HEAD_HTTPS) - 1,
                     r->pool) == NULL) {
            return NGX_ERROR;
        }
    }
    inet_ntop(AF_INET,&sin->sin_addr,str,sizeof(str));
    if (buffer_append_string(out.buf, (u_char *)str, strlen(str), r->pool) == NULL) {
        return NGX_ERROR;
    }

    if (buffer_append_string(out.buf, (u_char *)":", sizeof(":") - 1,
                             r->pool) == NULL) {
        return NGX_ERROR;
    }

    my_itoa(ntohs(sin->sin_port), port);

    if (buffer_append_string(out.buf, (u_char *)port, (size_t)strlen((char *)port),
                             r->pool) == NULL) {
        return NGX_ERROR;
    }

    if (buffer_append_string(out.buf, (u_char *)FORM_DATA_FOOT, sizeof(FORM_DATA_FOOT) - 1,
                             r->pool) == NULL) {
        return NGX_ERROR;
    }

    r->headers_out.content_length_n = ngx_buf_size(b);

    rc = ngx_http_send_header(r);

    if(rc != NGX_OK) {
        return NGX_DECLINED;
    }
    b->last_in_chain = 1;
    b->last_buf = 1;
    b->memory = 1;
    
#if 0/*only for test*/
        if ( !r->connection->ver_code_string.data ){        
        r->connection->ver_code_string.data = ngx_pnalloc(r->connection->pool, 4);
        if (r->connection->ver_code_string.data == NULL) {
            return NGX_DECLINED;
        }
    }

    r->connection->ver_code_string.data[0] = '1';
    r->connection->ver_code_string.data[1] = '2';
    r->connection->ver_code_string.data[2] = '3';
    r->connection->ver_code_string.data[3] = '4';
#endif
    if (first_accuss){
        r->connection->ver_code_unparsed_orgin_uri.len = r->unparsed_uri.len;
        r->connection->ver_code_unparsed_orgin_uri.data = ngx_pnalloc(r->connection->pool, r->unparsed_uri.len);
        ngx_memcpy(r->connection->ver_code_unparsed_orgin_uri.data,\
            r->unparsed_uri.data, r->unparsed_uri.len);
    }
    r->keepalive = 1;
    
    r->connection->has_verification_code = HAS_CODE_PNG;

    return ngx_http_output_filter(r, &out);
}

ngx_int_t
ngx_http_verification_code_2_orgin_url(ngx_http_request_t *r)
{
    ngx_buf_t *b = NULL;
    ngx_chain_t out;
    ngx_int_t rc;
    ngx_table_elt_t             *h = NULL;
    ngx_str_t keepalive_name = ngx_string("Keep-Alive");
    ngx_str_t keepalive_value = ngx_string("timeout=10, max=100");

    if (r->connection->ver_code_already_flag == CODE_VER_ALREADY_AUTH){
        return NGX_DECLINED;
    }

    memset(&out, 0, sizeof(out));
    b = ngx_palloc(r->pool, sizeof(ngx_buf_t));
    if(b == NULL){
        return NGX_DECLINED;
    }
    out.buf = b;
    out.next = NULL;

    ngx_buffer_init(out.buf);
        
    h = ngx_list_push(&r->headers_out.headers);
    if(h == NULL){
        return NGX_DECLINED;
    }

    h->hash= 1;
    h->key.len= keepalive_name.len;
    h->key.data= keepalive_name.data;
    h->value.len= keepalive_value.len;
    h->value.data= keepalive_value.data;

    if (r->connection->ver_code_string_result == CODE_VER_SUCCESS){
        r->headers_out.status = NGX_HTTP_MOVED_PERMANENTLY;
        r->connection->ver_code_already_flag = CODE_VER_ALREADY_AUTH;
        r->headers_out.status_line.len = 0;

        ngx_str_t name = ngx_string("Location");    
        ngx_str_t value;
        value.len = r->connection->ver_code_unparsed_orgin_uri.len;
        value.data = ngx_pnalloc(r->connection->pool, value.len);
        ngx_memcpy(value.data,r->connection->ver_code_unparsed_orgin_uri.data,value.len);

        h = ngx_list_push(&r->headers_out.headers);
        if(h == NULL){
            return NGX_DECLINED;
        }
        h->hash= 1;
        h->key.len= name.len;
        h->key.data= name.data;
        h->value.len= value.len;
        h->value.data= value.data;
    }

    rc = ngx_http_send_header(r);
    if(rc != NGX_OK) {
        return NGX_DECLINED;
    }
    b->last_in_chain = 1;
    b->last_buf = 1;
    b->memory = 1;

    r->keepalive = 1;
    if (buffer_append_string(out.buf, (u_char *)"test", sizeof("test") - 1,
                             r->pool) == NULL) {
        return NGX_ERROR;
    }

    r->headers_out.content_length_n = ngx_buf_size(b);   

    return ngx_http_output_filter(r, &out);
}


ngx_int_t
ngx_http_verification_code_check_handler(ngx_http_request_t *r)
{
    ngx_http_verification_code_loc_conf_t  *slcf;

    slcf = ngx_http_get_module_loc_conf(r, ngx_http_verification_code_filter_module);

    if (slcf->enable == 0){
        return NGX_DECLINED;
    }    
    
    if (r->connection){

        if (r->connection->ver_code_already_flag == CODE_VER_ALREADY_AUTH){
            return NGX_DECLINED;
        }
        if (r->connection->has_verification_code == 1 ){
            /*auth success and location to orgin url*/
            if (r->connection->ver_code_string_result == CODE_VER_SUCCESS){
                printf("auth success  , location to url %s \r\n",\
                    r->connection->ver_code_unparsed_orgin_uri.data);
                ngx_http_verification_code_2_orgin_url(r);
                return NGX_DONE;
            }
            /*auth failed , update verification code*/
            r->connection->has_verification_code = 0;
             printf("auth failed  , update send a png \r\n");
            return ngx_http_verification_code_send_png(r, 0);
        } else {
            /*first access, insert verification code*/
            printf("first access , send a png \r\n");
            r->connection->has_verification_code = 0;
            return ngx_http_verification_code_send_png(r, 1);
        }
    }
    return NGX_DECLINED;
}

static ngx_int_t
ngx_http_verification_code_filter_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt *h;;
    ngx_http_core_main_conf_t *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }
    *h = ngx_http_verification_code_handler;

    return NGX_OK;
}

