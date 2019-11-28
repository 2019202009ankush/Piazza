1. Connection Establish
	{
		"type":"client/server"
	}

2. Message Format for Create User
	{
		"purpose":"create_user",
		"username":"",
		"password":""
	}

3. Login
	{
		"purpose":"login",
		"username":"",
		"password":""
	}
	
4. Logout
	{
		"purpose":"logout",
		"username":""
	}
	
5. Get
	{
		"purpose":"get",
		"key":""
	}
	
6. Get Response
	{
		"value":"If value exist then value else failure"
	}

7. Create_user Response
	{
		"value":"exists/success"
	}

8. Delete
	{
		"purpose":"delete",
		"key":""
	}

9. Delete Response
	{
		"value":"success/failure/nexists"
	}

10. Put
	{
		"purpose":"put",
		"key":"",
		"value":""
	}
	
11. Update
	{
		"purpose":"update",
		"key":"",
		"value":""
	}

12. Value in Put/Update (Nested JSON String)
	{
		"col_name":"col_value",
		"col_name":"col_value",
		.
		.
		.
	}

13. Put n Update Response
	{
		"value":"success/exists/failure"
	}
