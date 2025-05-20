<?php
session_start();

?>

<!DOCTYPE html>
<html>
    <head>
        <title>PHP Set session script</title>
    </head>
    <body>
		<div>
        <?php
			echo '<h1>PHP Set session script was executed</h1>';
			$_SESSION["color"] = $_POST["color"];
			echo "Current session data :";
			print_r($_SESSION);
		?>
		</div>
		<p>Go back : <a href="/app/sessionpage1.php">CGI session page 1</a>, <a href="/app/sessionpage1.php">CGI session page 2</a></p>
    </body>
</html>