CREATE TABLE users (id INT, username TEXT);
INSERT INTO users VALUES (1, 'Alice');
INSERT INTO users VALUES (2, 'Bob');
INSERT INTO users VALUES (3, 'Charlie');
INSERT INTO users VALUES (4, 'Dave');
INSERT INTO users VALUES (5, 'Eve');
SELECT * FROM users WHERE id > 2;
SELECT * FROM users WHERE id < 4;
SELECT * FROM users WHERE id > 0;
SELECT * FROM users WHERE id < 10;
SELECT * FROM users WHERE id > 5;
SELECT * FROM users WHERE id < 1;
.exit
