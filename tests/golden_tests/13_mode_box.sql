CREATE TABLE users (id INT, username TEXT);
INSERT INTO users VALUES (1, 'Alice');
INSERT INTO users VALUES (2, 'Bob');
INSERT INTO users VALUES (3, 'Charlie-with-a-long-name');
.mode box
SELECT * FROM users;
.mode plain
SELECT * FROM users;
.exit
