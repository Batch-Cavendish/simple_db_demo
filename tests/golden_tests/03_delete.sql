CREATE TABLE users (id INT, username TEXT);
INSERT INTO users VALUES (1, 'Alice');
INSERT INTO users VALUES (2, 'Bob');
SELECT * FROM users;
DELETE FROM users WHERE id = 1;
SELECT * FROM users;
DELETE FROM users WHERE id = 2;
SELECT * FROM users;
DELETE FROM users WHERE id = 3;
.exit
