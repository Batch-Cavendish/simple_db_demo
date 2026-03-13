CREATE TABLE users (id INT, username TEXT);
INSERT INTO users VALUES (1, 'Alice');
INSERT INTO users VALUES (2, 'Bob');
SELECT * FROM users;
UPDATE users SET username = 'Charlie' WHERE id = 1;
SELECT * FROM users;
UPDATE users SET username = 'Dave' WHERE id = 3;
SELECT * FROM users;
.exit
