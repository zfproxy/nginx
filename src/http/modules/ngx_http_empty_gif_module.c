
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


/*
 * ngx_http_empty_gif_module.c
 *
 * 该模块实现了返回一个最小的单像素透明GIF图像的功能。
 *
 * 支持的功能:
 * - 返回一个43字节的透明GIF图像
 * - 自动设置Content-Type为image/gif
 * - 支持HEAD和GET请求方法
 * - 设置Last-Modified头为固定时间戳
 *
 * 支持的指令:
 * - empty_gif: 启用空GIF模块功能
 *   语法: empty_gif;
 *   默认值: -
 *   上下文: location
 *
 * 支持的变量:
 * 该模块不提供任何变量。
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/**
 * @brief 处理empty_gif指令的函数
 *
 * 这个函数用于处理nginx配置文件中的empty_gif指令。
 * 当在location块中使用empty_gif指令时，会调用此函数进行相应的设置。
 *
 * @param cf 指向nginx配置结构的指针
 * @param cmd 指向当前命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
static char *ngx_http_empty_gif(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static ngx_command_t  ngx_http_empty_gif_commands[] = {

    { ngx_string("empty_gif"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_empty_gif,
      0,
      0,
      NULL },

      ngx_null_command
};


/* the minimal single pixel transparent GIF, 43 bytes */

static u_char  ngx_empty_gif[] = {

    'G', 'I', 'F', '8', '9', 'a',  /* header                                 */

                                   /* logical screen descriptor              */
    0x01, 0x00,                    /* logical screen width                   */
    0x01, 0x00,                    /* logical screen height                  */
    0x80,                          /* global 1-bit color table               */
    0x01,                          /* background color #1                    */
    0x00,                          /* no aspect ratio                        */

                                   /* global color table                     */
    0x00, 0x00, 0x00,              /* #0: black                              */
    0xff, 0xff, 0xff,              /* #1: white                              */

                                   /* graphic control extension              */
    0x21,                          /* extension introducer                   */
    0xf9,                          /* graphic control label                  */
    0x04,                          /* block size                             */
    0x01,                          /* transparent color is given,            */
                                   /*     no disposal specified,             */
                                   /*     user input is not expected         */
    0x00, 0x00,                    /* delay time                             */
    0x01,                          /* transparent color #1                   */
    0x00,                          /* block terminator                       */

                                   /* image descriptor                       */
    0x2c,                          /* image separator                        */
    0x00, 0x00,                    /* image left position                    */
    0x00, 0x00,                    /* image top position                     */
    0x01, 0x00,                    /* image width                            */
    0x01, 0x00,                    /* image height                           */
    0x00,                          /* no local color table, no interlaced    */

                                   /* table based image data                 */
    0x02,                          /* LZW minimum code size,                 */
                                   /*     must be at least 2-bit             */
    0x02,                          /* block size                             */
    0x4c, 0x01,                    /* compressed bytes 01_001_100, 0000000_1 */
                                   /* 100: clear code                        */
                                   /* 001: 1                                 */
                                   /* 101: end of information code           */
    0x00,                          /* block terminator                       */

    0x3B                           /* trailer                                */
};


static ngx_http_module_t  ngx_http_empty_gif_module_ctx = {
    NULL,                          /* preconfiguration */
    NULL,                          /* postconfiguration */

    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */

    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */

    NULL,                          /* create location configuration */
    NULL                           /* merge location configuration */
};


ngx_module_t  ngx_http_empty_gif_module = {
    NGX_MODULE_V1,
    &ngx_http_empty_gif_module_ctx, /* module context */
    ngx_http_empty_gif_commands,   /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};


/**
 * @brief 定义空GIF图像的MIME类型
 *
 * 这个静态变量定义了空GIF图像的MIME类型。
 * 使用ngx_string宏将字符串"image/gif"转换为ngx_str_t类型。
 * 在发送HTTP响应时，这个变量用于设置Content-Type头部。
 */
static ngx_str_t  ngx_http_gif_type = ngx_string("image/gif");


/* 
 * 定义一个静态函数 ngx_http_empty_gif_handler
 * 该函数用于处理空 GIF 图像的 HTTP 请求
 * 返回类型为 ngx_int_t，表示处理结果的状态码
 */
static ngx_int_t
ngx_http_empty_gif_handler(ngx_http_request_t *r)
{
    ngx_http_complex_value_t  cv;

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    ngx_memzero(&cv, sizeof(ngx_http_complex_value_t));

    cv.value.len = sizeof(ngx_empty_gif);
    cv.value.data = ngx_empty_gif;
    r->headers_out.last_modified_time = 23349600;

    return ngx_http_send_response(r, NGX_HTTP_OK, &ngx_http_gif_type, &cv);
}


static char *
ngx_http_empty_gif(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_empty_gif_handler;

    return NGX_CONF_OK;
}
