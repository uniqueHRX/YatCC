import uuid
import xml.etree.ElementTree as ET

from openai import OpenAI
from typing import Callable, List, Literal


def remove_deepseek_r1_think(s: str) -> str:
    if "</think>" in s:
        s = s.split("</think>", 1)[1].strip()
    return s


def remove_md_block_marker(marker: str):
    def remove(s: str) -> str:
        import re
        s = s.strip()
        # 匹配开头的 ```marker 或 '''marker（可能带空白）
        start_pattern = re.compile(
            r"^[`'']{3}\s*" + re.escape(marker) + r"\s*\n?"
        )
        s = start_pattern.sub("", s)
        # 匹配结尾的 ``` 或 '''（可能带空白）
        s = re.sub(r"\n?\s*[`'']{3}\s*$", "", s)
        return s

    return remove


def extract_text_from_xml(tag: str):
    def extract(s: str) -> str:
        # 尝试 XML 解析
        try:
            et = ET.fromstring(s)
            node = et.find(tag)
            if node is not None and node.text is not None:
                return node.text.strip()
        except ET.ParseError:
            pass  # XML 解析失败，fallback 到正则

        # 正则 fallback：直接从文本中提取标签内容
        import re
        pattern = f"<{tag}>\\s*(.*?)\\s*</{tag}>"
        match = re.search(pattern, s, re.DOTALL)
        if match:
            return match.group(1).strip()

        raise ValueError(
            f"无法从响应中提取 tag 为 {tag} 的内容，响应前500字符: {s[:500]}"
        )

    return extract


def extract_xml_tag(xml_str: str, tag: str) -> str:
    """从 XML 字符串中提取指定标签的文本内容"""
    return extract_text_from_xml(tag)(xml_str)


class LLMHelperImpl:
    _instances = {}
    _instances_init = {}

    def __new__(cls, api_key: str, base_url: str):
        key = (api_key, base_url)
        if key not in cls._instances:
            instance = super(LLMHelperImpl, cls).__new__(cls)
            cls._instances[key] = instance
            cls._instances_init[key] = False
        return cls._instances[key]

    def __init__(self, api_key: str, base_url: str) -> None:
        if not LLMHelperImpl._instances_init[(api_key, base_url)]:
            self.__client = OpenAI(api_key=api_key, base_url=base_url)
            self.__sessions = {}
            LLMHelperImpl._instances_init[(api_key, base_url)] = True

    def create_new_session(self) -> str:
        session_id = str(uuid.uuid4())
        self.__sessions[session_id] = []
        return session_id

    def delete_session(self, session_id: str):
        del self.__sessions[session_id]

    def add_content(
        self,
        session_id: str,
        role: Literal["user", "system", "assistant"],
        content: str,
    ) -> None:
        self.__sessions[session_id].append({"role": role, "content": content})

    def chat(
        self,
        session_id: str,
        model: str,
        handlers: List[Callable[[str], str]] = [],
        **params,
    ) -> str:
        messages = self.__sessions[session_id]
        response = (
            self.__client.chat.completions.create(
                messages=messages, model=model, **params
            )
            .choices[0]
            .message.content
        )

        for handler in handlers:
            try:
                response = handler(response)
            except Exception:
                import sys
                print(
                    f"[DEBUG] Handler failed. "
                    f"Current response (first 800 chars):\n{response[:800]}",
                    file=sys.stderr,
                )
                raise
        return response
