CREATE TABLE users (id INT, username TEXT);
CREATE TABLE products (id INT, name TEXT, price INT);
INSERT INTO users VALUES (1, 'Alice');
INSERT INTO users VALUES (2, 'Bob');
INSERT INTO products VALUES (1, 'Laptop', 1000);
INSERT INTO products VALUES (2, 'Mouse', 20);
SELECT * FROM users;
SELECT * FROM products;
SELECT * FROM users WHERE id = 1;
SELECT * FROM products WHERE id = 2;
.exit
