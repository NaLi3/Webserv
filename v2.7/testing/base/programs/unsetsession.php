<?php
session_start();

?>

<!DOCTYPE html>
<html>
    <head>
        <title>PHP Unset session script</title>
    </head>
    <body>
		<div>
        <?php
			echo '<h1>PHP Unset session script was executed</h1>';
			echo "Current session data was:";
			print_r($_SESSION);
			echo "It is now deleted";
			session_unset();
			session_destroy();
			// To also delete the PHPSESSID cookie (from PHP man), not necessary but interesting
			// if (ini_get("session.use_cookies"))
			// {
			// 	$params = session_get_cookie_params();
			// 	setcookie(session_name(), '', time() - 42000,
			// 		$params["path"], $params["domain"],
			// 		$params["secure"], $params["httponly"]
			// 	);
			// }

		?>
		</div>
		<p>Go back : <a href="/app/sessionpage1.php">CGI session page 1</a>, <a href="/app/sessionpage1.php">CGI session page 2</a></p>
    </body>
</html>