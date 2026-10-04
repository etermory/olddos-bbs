string do_alias(string arg);

string write_prompt(int num)
{
	string prompt;
	int mhp, hp, msp, sp;

	mhp = this_object()->query("최대체력");
	msp = this_object()->query("최대정신력");
	hp = this_object()->query("체력");
	sp = this_object()->query("정신력");

	prompt = "[ "+hp+"/"+mhp+","+sp+"/"+msp+" ]\n> ";

	if( num ) return prompt;
	tell_object(this_object(),prompt);
	return "";
}

static nomask string process_input(string arg)
{
	int x;
	string fm;

	x = strlen(arg);
	fm = arg[x-1..];

	if( fm == "." || fm == "?" || fm  == " ") return arg+" 말";
	else if( arg == "!" ) return this_object()->query_temp("이전명령");
	else {
		arg = do_alias(arg);
		if( arg && arg != "" )
			this_player()->set_temp("이전명령",arg);
		return arg;
	}
}
