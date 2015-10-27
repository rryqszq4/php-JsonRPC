/**
gcc -o curl-epoll curl-epoll.c ../jsr_curl.c ../jsr_list.c ../jsr_epoll.c -lcurl
*/

#include "../jsr_curl.h"
#include "../jsr_epoll.h"

int
socket_callback(CURL *easy, curl_socket_t fd, int action, void *u, void *s)
{
	printf(">>> %s: adding fd=%d action=%d\n", __func__, fd, action);
	jsr_epoll_t *jsr_epoll = (jsr_epoll_t *) u;

	if (action == CURL_POLL_REMOVE)
	{
		jsr_epoll_del_fd(jsr_epoll, fd);
	}

	if (action == CURL_POLL_IN || action == CURL_POLL_INOUT)
	{
		printf("in\n");
		jsr_epoll_add_fd(jsr_epoll, fd);
		jsr_epoll_set_in(jsr_epoll, fd);
	}

	if (action == CURL_POLL_OUT || action == CURL_POLL_INOUT)
	{
		printf("out\n");
		jsr_epoll_add_fd(jsr_epoll, fd);
		jsr_epoll_set_out(jsr_epoll, fd);
	}

	return 0;
}

int 
timer_callback(CURLM *multi, long timeout_ms, void *u)
{
	printf(">>> %s: timeout: %ld ms\n", __func__, timeout_ms);
  	return 0;
}

int 
main()
{
	jsr_curl_global_new();

	jsr_epoll_t *jsr_epoll = jsr_epoll_init();

	jsr_curlm_t *jsr_curlm = jsr_curlm_new();

	curl_multi_setopt(jsr_curlm->multi_handle, CURLMOPT_SOCKETFUNCTION, socket_callback);
	curl_multi_setopt(jsr_curlm->multi_handle, CURLMOPT_SOCKETDATA, jsr_epoll);
	curl_multi_setopt(jsr_curlm->multi_handle, CURLMOPT_TIMERFUNCTION, timer_callback);

	jsr_curl_item_t *jsr_curl_item = jsr_curl_item_new(
			"http://yaf-lib.com/",
			"key=value",
			9
		);
	jsr_curl_item_setopt(jsr_curl_item);
	jsr_curlm_list_append(jsr_curlm, jsr_curl_item);
	
	jsr_curl_item_t *jsr_curl_item_2 = jsr_curl_item_new(
			"http://yaf-lib.com/",
			"key=value",
			9
		);
	jsr_curl_item_setopt(jsr_curl_item_2);
	jsr_curlm_list_append(jsr_curlm, jsr_curl_item_2);

	jsr_curl_item_t *jsr_curl_item_3 = jsr_curl_item_new(
			"http://yaf-lib.com/",
			"key=value",
			9
		);
	jsr_curl_item_setopt(jsr_curl_item_3);
	jsr_curlm_list_append(jsr_curlm, jsr_curl_item_3);

	jsr_curl_item_t *jsr_curl_item_4 = jsr_curl_item_new(
			"http://yaf-lib.com/",
			"key=value",
			9
		);
	jsr_curl_item_setopt(jsr_curl_item_4);
	jsr_curlm_list_append(jsr_curlm, jsr_curl_item_4);

	jsr_curl_item_t *jsr_curl_item_5 = jsr_curl_item_new(
			"http://yaf-lib.com/",
			"key=value",
			9
		);
	jsr_curl_item_setopt(jsr_curl_item_5);
	jsr_curlm_list_append(jsr_curlm, jsr_curl_item_5);

	jsr_curl_item_t *jsr_curl_item_6 = jsr_curl_item_new(
			"http://yaf-lib.com/",
			"key=value",
			9
		);
	jsr_curl_item_setopt(jsr_curl_item_6);
	jsr_curlm_list_append(jsr_curlm, jsr_curl_item_6);

	jsr_curlm_post(jsr_curlm);

	int running_handles = 1;
	int loop_total = 0;
	while (running_handles > 0)
	{
		printf(">>> calling epoll_wait\n");
		loop_total = jsr_epoll_loop(jsr_epoll , 1000);
		printf("%d\n", loop_total);

		if (loop_total == 0){
			curl_multi_socket_action(jsr_curlm->multi_handle, CURL_SOCKET_TIMEOUT, 0, &running_handles);
		}
		else 
		{
			int i = 0;
			for (i = 0; i < loop_total; i++){
				curl_multi_socket_action(jsr_curlm->multi_handle, jsr_epoll->events[i].data.fd, 0, &running_handles);
			}
		}

	}

	//curl_multi_setopt(jsr_curl->multi_handle, CURLMOPT_SOCKETFUNCTION, sock_cb);
	//curl_multi_setopt(jsr_curl->multi_handle, CURLMOPT_SOCKETDATA, jsr_curl);

	jsr_epoll_destroy(&jsr_epoll);

	jsr_curlm_destroy(&jsr_curlm);
	jsr_curl_global_destroy();

	return 0;
}
