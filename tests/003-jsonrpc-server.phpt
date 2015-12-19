--TEST--
Check for jsonrpc presence
--SKIPIF--
<?php if (!extension_loaded("jsonrpc")) print "skip"; ?>
--FILE--
<?php 
	$server = new Jsonrpc_Server();

// Procedures registration

        /*$server->register('addition', function ($a, $b) {
            return $a + $b;
        });*/

// Return the response to the client
        //$server->execute();
?>
--EXPECT--