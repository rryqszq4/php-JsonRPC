<?php

$http = new swoole_http_server("127.0.0.1", 9501);

function add($a, $b){
        return $a + $b;
}

$http->on('Request', function($request, $response){
        if ($request->server['request_uri'] == "/jsonrpc_server"){
        $payload = $request->rawContent();
                $jsr_server = new Jsonrpc_Server($payload);
                $jsr_server->register('addition', 'add');
                $res = $jsr_server->execute();
                //var_dump($res);
                $response->end($res);
                unset($payload);
                unset($jsr_server);
                unset($res);
        }else {
                //var_dump($request);
                $response->end("error");
        }
});
$http->start();
?>