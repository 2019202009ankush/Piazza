1. Connection Establish
	{
		"type":"client/server"
	}

2. Message Format for Create User
	{
		"purpose":"create_user"
		"username":""
		"password":""
	}

3. Login
	{
		"purpose":"login"
		"username":""
		"password":""
	}
	
4. Logout
	{
		"purpose":"logout"
		"username":""
	}
	
5. Get
	{
		"purpose":"get"
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
		"purpose":"delete"
		"key":""
	}

9. Delete Response
	{
		"value":"success/failure/nexists"
	}
